CC=clang
CFLAGS=-std=c99 -Wall -pedantic
LFLAGS=

all: export build logsRemove

export:
    export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/Users/tretikoff/Desktop/pa2";
    LD_PRELOAD=/Users/tretikoff/Desktop/pa2/lib64/libruntime.so ./a.out –p 2 10 20;

build:
	$(CC) $(CFLAGS) *.c $(LFLAGS)

logsRemove:
	rm -r *.log

clean:
	killall -9 a.out
