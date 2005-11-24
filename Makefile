CC=g++
CFLAGS=-Wall -ansi -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=22 -DRLOG_COMPONENT="loggedfs"
LDFLAGS=-Wall -ansi -lpcre -lfuse -lrlog


all: loggedfs
loggedfs: loggedfs.o Config.o
	$(CC) -o loggedfs loggedfs.o Config.o $(LDFLAGS)

loggedfs.o: loggedfs.cpp
	$(CC) -o loggedfs.o -c loggedfs.cpp $(CFLAGS)

Config.o: Config.cpp Config.h
	$(CC) -o Config.o -c Config.cpp $(CFLAGS)

clean:
	rm -rf *.o

mrproper: clean
	rm -rf hello