Uniqify
=================
Multiprocess Unique Word Finder in C

Overview
-------------
Uniqify reads a text file and outputs the unique words in the file, sorted in alphabetic order, with the count of the occurrence of each word. It uses multiple processes to parallelize the word sorting and counting, and creates a series of pipes to communicate between the sub-processes that do the sorting.

Usage
-------------

```./uniqify -c [# of processes] < inputfile```
