#@rm -f ar-ctoc.txt ar-vtoc.txt myar-ctoc.txt myar-vtoc.txt

EXE = uniqify
CC = gcc
DEBUG =-g
OPTIMIZE = -Os
CFLAGS = -Wall $(DEBUG) $(OPTIMIZE)
SRC =	uniqify.c
OBJ = $(SRC:.c=.o)

all: $(EXE)

$(EXE): $(OBJ)
	@$(CC) -o $(EXE) $(CFLAGS) $(OBJ)

$(OBJ) : %.o : %.c
	@$(CC) -c $(CFLAGS) $< -o $@

clean:
	@rm -f $(OBJ)
	@rm -f $(EXE)
	@echo Done cleaning
