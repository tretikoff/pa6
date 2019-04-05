CC=clang
CFLAGS=-std=c99 -Wall -pedantic
LFLAGS=-L. -lruntime

all: export build logsRemove

export:
    export LD_LIBRARY_PATH="/mnt/c/Users/treti/Downloads/pa6";
    LD_PRELOAD=/mnt/c/Users/treti/Downloads/pa6/libruntime.so ./a.out –p 2 10 20;

build:
	$(CC) $(CFLAGS) *.c $(LFLAGS)

logsRemove:
	rm -r *.log

clean:
	killall -9 a.out
