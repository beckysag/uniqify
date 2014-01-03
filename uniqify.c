#define _POSIX_C_SOURCE 2

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sched.h>

#ifndef BUF_SIZE
#define BUF_SIZE 512
#endif

#define SORTPATH "/bin/sort"

struct word_map {
	char word[BUF_SIZE];
	int idx;
};

/* SIGCHLD handler. */
void sigchld_handler(int sig);

/* SIGPIPE handler. */
void sigpipe_handler(int sig);

/* convert word to lowercase */
void to_lower(char *str);

/* get index of lowest string in array */
int get_alpha(char **arr, int count);

void usage(){printf("Usage: uniqify [-c \x1b[04mnum-pipes\x1b[24m]\n"); }

/* redirect I/O from old_fd to new_fd */
void redirect(int old_fd, int new_fd);

/* Uses fputs to write msg to stderr, the exits with status of EXIT_FAILURE */
void putmsg_exit(char* msg);


int main(int argc, char *argv[]) {
	int pid2;
	int status;
	int fd_out;
	int i,j; 										// loop variables
	char **words;
	int **fd_pipeA;
	int **fd_pipeB;
	FILE *supp_out_stream;
	FILE **in_stream;
	char *tmp_word;
	int n,b;
	int idx = 0;
	int num_sorters = 0;
	char curr[BUF_SIZE];
	char prev[BUF_SIZE];
	int currIdx = 0;
	int *kids;


	// Process command line option arguments
	if (argc < 3){
		usage();
		exit(EXIT_FAILURE);
	}
	else if (argc == 3){
		if (strcmp(argv[1],"-c") == 0)
			num_sorters = atoi(argv[2]);
	}
	else {
		usage();
		exit(EXIT_FAILURE);
	}

	kids = (int*)malloc(num_sorters * sizeof(int));
	FILE *parents_out_stream[num_sorters];
	int num_open_streams = num_sorters;					// number of streams still open


	fd_pipeA = (int**)malloc(num_sorters * sizeof(int*));
	for (i=0; i < num_sorters; i++){
		fd_pipeA[i] = (int*)malloc(2 * sizeof(int));
	}

	fd_pipeB = (int**)malloc(num_sorters * sizeof(int*));
	for (i=0; i < num_sorters; i++){
		fd_pipeB[i] = (int*)malloc(2 * sizeof(int));
	}


	// Install SIGCHLD handler
	struct sigaction act;							/* new signal dispostion*/
	memset(&act, 0, sizeof(act));
	act.sa_handler = sigchld_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGCHLD, &act, NULL);					/* install the handler */

	// Install SIGPIPE handler
	struct sigaction act_pipe;							/* new signal dispostion*/
	memset(&act_pipe, 0, sizeof(act_pipe));
	act_pipe.sa_handler = sigpipe_handler;
	sigemptyset(&act_pipe.sa_mask);
	act_pipe.sa_flags = 0;
	sigaction(SIGPIPE, &act_pipe, NULL);					/* install the handler */


	// create all the pipe A's
	for (i=0; i < num_sorters; i++)
		if (pipe(fd_pipeA[i]) < 0)							// parent creates pipe A for sorter-child i
			putmsg_exit("Error creating pipes\n");


	// create suppressor.
	switch (fork()) {
	case -1:
		putmsg_exit("Error while trying create suppressor \n");
		break;

	case 0:	// suppressor executes here

		fd_out = dup(STDOUT_FILENO);					// copy stdout descriptor for later

		// create the pipes to connect to the sorter processes
		for (i=0; i < num_sorters; i++){

			if (pipe(fd_pipeB[i]) < 0)					// supp creates pipe B[i] for each sorter-child 0 to i
				putmsg_exit("Error creating pipes\n");

			// create sorter grandchild
			switch(pid2=fork()){
			case -1:
				putmsg_exit("Error while trying create sorter\n");
				break;

			// sorter processes execute here
			case 0:
				/* close unnecessary copy of parent's stdout */
				close(fd_out);

				/* bind sorter-child's input to the read end of the sorter pipe*/
				redirect(fd_pipeA[i][0], STDIN_FILENO);

				/* bind child's output to the write end of suppressor pipe (supress_fd) */
				redirect(fd_pipeB[i][1], STDOUT_FILENO);

				if ((close(fd_pipeB[i][0]) < 0) || (close(fd_pipeA[i][1]) < 0)) // close unnecessary file descriptor
					putmsg_exit("Error trying to close file descriptors\n");

				// Close read side of each pipe B for siblings created before
				for (j = 0; j < i; j++)
						if (close(fd_pipeB[j][0]) < 0 )
							putmsg_exit("Error trying to close sorter file descriptor in pipe B\n");

				// Close pipe A descriptors meant for siblings created after
				for (j = i+1; j < num_sorters; j++)
					if ((close(fd_pipeA[j][0]) < 0) || (close(fd_pipeA[j][1]) < 0))
						putmsg_exit("Error trying to close sorter file descriptor in pipe A\n");

				execl(SORTPATH, "/bin/sort", (char*)0);
				fputs("Error execing sort\n", stderr);
				exit(EXIT_FAILURE);
				break;

			default: // back to suppressor
				kids[i] = pid2;
				// close both descriptors to pipe A, and the write side of pipe B
				if ((close(fd_pipeA[i][0]) < 0)
						|| (close(fd_pipeA[i][1]) < 0)
						|| (close(fd_pipeB[i][1]) < 0)) {
					putmsg_exit("Error trying to close suppressor file descriptors\n");
				}
				break;
			}
		}// end for-loop that creates sorter-children
		// suppressor done forking; deal with things specific to the suppressor

		if ((close(STDIN_FILENO) < 0) || (close(STDOUT_FILENO) < 0))				// close standard input & output
			putmsg_exit("Error trying to close file descriptor\n");

		supp_out_stream = fdopen(fd_out, "w");										// open output stream
		in_stream = (FILE**)malloc(num_sorters*sizeof(FILE*));
		for (i = 0; i < num_sorters; i++)
			in_stream[i] = fdopen(fd_pipeB[i][0], "r");								// open stream to read from each sorter-child


		// allocate buffers to hold current words from streams
		char **words_out = (char**) malloc(num_sorters * sizeof(char*));
		for (n=0; n<num_sorters; n++)
			words_out[n] = (char*)malloc(BUF_SIZE * sizeof(char));


		// read first words from each stream into its buffer
		for (n=0; n<num_sorters; n++)
			if (fgets(words_out[n], BUF_SIZE, in_stream[n]) == NULL) {
				waitpid(kids[n], NULL, 0);
				while (fgets(words_out[n], BUF_SIZE, in_stream[n]) == NULL)
					continue;
			}


		b = get_alpha(words_out, num_open_streams);								// initialize prev with first word
		strcpy(prev, words_out[b]);
		currIdx = 0;

		// while at least 1 stream still has data left to read
		while (num_open_streams > 0) {
			b = get_alpha(words_out, num_open_streams);							// find idx of buffer with (alphabetically) smallest value
			strcpy(curr, words_out[b]);

			if ((curr != NULL) && strlen(curr) > 1) {
				if (strcmp(curr, prev) != 0){ //if it's a new word
					if (prev != NULL){
						fprintf(supp_out_stream, "%7i ", currIdx);
						fputs(prev, supp_out_stream);											// print it
					}
					strcpy(prev, curr);
					currIdx = 1;
				}else {
					currIdx++;
				}
			}
			// replace words_out[b] with next word from its stream
			if (fgets(words_out[b], BUF_SIZE, in_stream[b]) == NULL){
				// remove this stream from the array of streams
				FILE **tmp = in_stream;												// save a pointer to the original stream
				in_stream = malloc((num_open_streams - 1) * sizeof(FILE*));			// reallocate new array of streams for 1 less than the origianl
				idx = 0;
				for (i=0; i< num_open_streams; i++){ 								// copy remaining streams into newly allocated memory
					if (i != b){
						in_stream[idx] = tmp[i];
						idx++;
					}
				}
				fclose(tmp[b]);														//close the stream
				free(tmp);															// free original array of file poitners
				// remove the buffer from array of buffers
				tmp_word = words_out[b];
				for (i=b; i<num_open_streams-1; i++)
					words_out[i] = words_out[i+1];
				words_out[num_open_streams-1] = tmp_word;
				num_open_streams--;													// update number of open streams variable
			}

		}//end while

		fprintf(supp_out_stream, "%7d ", currIdx);
		fputs(prev, supp_out_stream);											// print last word



		for (n=0; n<num_sorters; n++)
			free(words_out[n]);
		free(words_out);

		// free allocated memory & close file streams/descriptors (fclose(in_stream[i]) freed above
		for (i=0; i < num_sorters; i++)
			close(fd_pipeB[i][0]);

		free(kids);
		fclose(supp_out_stream);
		free(in_stream);
		close(fd_out);
		exit(EXIT_SUCCESS);
		break;



	// Parent process
	default:
		if (close(STDOUT_FILENO) < 0)							// close standard output
			putmsg_exit("Error closing standard output\n");

		for (i=0; i < num_sorters; i++) {
			if (close(fd_pipeA[i][0]) < 0)						// close unnecessary file descriptor
				putmsg_exit("Error closing file descriptor\n");
			parents_out_stream[i] = fdopen(fd_pipeA[i][1],"w");	// open stream to write to each pipe A
	//		setbuf(parents_out_stream[i], NULL);
		}


		// send input to stdout at pipe A
		words = (char**)malloc(num_sorters * sizeof(char*));
		for (i=0; i < num_sorters; i++)
			words[i] = (char*)malloc(BUF_SIZE * sizeof(char));

		j = 0;
		while(
			(n=fscanf(stdin, "%*[^A-Za-z]"), // suppress non-alpha characters
				fscanf(stdin, "%512[a-zA-Z]", words[j])) != EOF) {
			to_lower(words[j]);
			strcat(words[j], "\n");
	    	fputs(words[j], parents_out_stream[j]);
	    	if ((j + 1) == num_sorters) {
		    	fflush(parents_out_stream[j]);
		    	j = 0;
	    	}
	    	else j++;
	    }

		// Close pipes to grandchild sorters
		for (i=0; i < num_sorters; i++){
			fclose(parents_out_stream[i]);
			close(fd_pipeA[i][1]);
		}

		for (i=0; i < num_sorters; i++)
			free(words[i]);
		free(words);

		wait(&status); //wait for suppressor
		break;
	}

	for (i=0; i < num_sorters; i++){
		free(fd_pipeA[i]);
		free(fd_pipeB[i]);
	}
	free(fd_pipeA);
	free(fd_pipeB);
	return(0);
}







//******************************* Function definitions *******************************************

void putmsg_exit(char* msg){
	fputs(msg, stderr);
	exit(EXIT_FAILURE);
}

void redirect(int old_fd, int new_fd){
	if (old_fd != new_fd) {
		if (dup2(old_fd, new_fd) == -1) {	// redirect the input to come from pipe A's read side
			fputs("Error trying to redirect I/O to new file descriptor\n", stderr);
			exit(EXIT_FAILURE);
		}
		if (close(old_fd) < 0) {
			fputs("Error closing file descriptor\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
}

void sigchld_handler(int sig) {
	int savedErrno;
	pid_t childPid;
	savedErrno = errno;
	//int pid = getpid();
	int status;
	//fprintf(stderr, "In process %d -- handler: Caught SIGCHLD\n", pid);
	while ((childPid = waitpid(-1, &status, WNOHANG)) > 0) {
		/*
		fprintf(stderr, "handler: Reaped child %ld - status %d",(long) childPid, status);
		 if (WIFEXITED(status)) {
		                fprintf(stderr, "exited, status=%d\n", WEXITSTATUS(status));
		            } else if (WIFSIGNALED(status)) {
		                fprintf(stderr, "killed by signal %d\n", WTERMSIG(status));
		            } else if (WIFSTOPPED(status)) {
		                fprintf(stderr, "stopped by signal %d\n", WSTOPSIG(status));
		            } else if (WIFCONTINUED(status)) {
		                printf("continued\n");
		            }
		            */
		continue;
	}
	errno = savedErrno;
}


void sigpipe_handler(int sig) {
	int savedErrno;
	savedErrno = errno;
	fprintf(stderr, "handler: Caught SIGPIPE\n");
	errno = savedErrno;
}


void to_lower(char *str){
	int len = strlen(str);
	int i;
	for (i = 0; i < len; i++) {
		if (isalpha(str[i])) {
			str[i] = tolower(str[i]);
		}
	}
	str[len] = '\0';
}

// takes an array of count strings each less than max_len long
// finds the alphabetically smallest word and returns its position in the array
int get_alpha(char **arr, int count){
	int i, j, n;
	struct word_map *arrStruct;
	arrStruct = malloc(count * sizeof(struct word_map));

	for (j = 0; j < count; j++) {
		strcpy(arrStruct[j].word, arr[j]);		// copy word into struct
		arrStruct[j].idx = j;					// copy index into struct
	}
	// sort structs alphabetically by their word
	for (j = 0; j < count - 1; j++) {
		for (i = 0; i < count - 1; i++) {
			struct word_map tmp;
			// if next word is before this word, remove this word from list
			// put next word in its place, and then put it back in list at next spot
			if (0 < strcmp(arrStruct[i].word, arrStruct[i+1].word)) {
				strcpy(tmp.word, arrStruct[i].word);		// copy word into tmp struct
				tmp.idx= arrStruct[i].idx;
				arrStruct[i] = arrStruct[i + 1];
				arrStruct[i + 1] = tmp;
			}
		}
	}
	n = arrStruct[0].idx;		// save index before freeing arrStruct memory
	free(arrStruct);
	return n;
}
