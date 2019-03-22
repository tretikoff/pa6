CC=clang
CFLAGS=-std=c99 -Wall -pedantic
LFLAGS=-L. -lruntime

all: export build logsRemove

export:
    export LD_LIBRARY_PATH="/home/parallels/Desktop/Parallels\ Shared\ Folders/Home/Desktop/pa2";
    LD_PRELOAD=/home/parallels/Desktop/Parallels\ Shared\ Folders/Home/Desktop/pa2/libruntime.so ./a.out –p 2 10 20;

build:
	$(CC) $(CFLAGS) *.c $(LFLAGS)

logsRemove:
	rm -r *.log

clean:
	killall -9 a.out
