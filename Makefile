CFLAGS=-std=gnu11 -pedantic -Wextra -Wall -g
CC=gcc
all: malloc_test

malloc_test: main.o my_malloc.o
	$(CC) $(CFLAGS) -o $@ $^

my_malloc.o: my_malloc.c my_malloc.h
	$(CC) $(CFLAGS) -c $^
main.o: main.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm *.o *.gch malloc_test