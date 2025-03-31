# This is a Makefile for compiling lekhani.c
# To define the compiler use $(CC) which points to the c compiler used
# and in ubuntu the compiler used is `gcc`; CC = gcc

# Rule for building and running the target executable; (make/make lekhani) command
lekhani: lekhani.c
	$(CC) lekhani.c -o lekhani -Wall -Wextra -pedantic -std=c99

# Rule for running the target executable; (make run) command
run: lekhani
	./lekhani