CC=gcc
CFLAGS=-Wall -c

BINS=thread_manager.o libmythreads.a

all: $(BINS)

project2.o: thread_manager.c mythreads.h
	$(CC) $(CFLAGS) thread_manager.c

libmythreads.a: thread_manager.o 
	ar -cvr libmythreads.a thread_manager.o

clean:
	rm $(BINS)
