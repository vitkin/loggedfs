CC=g++
CFLAGS=-Wall -ansi -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=22 -DRLOG_COMPONENT="loggedfs"
LDFLAGS=-Wall -ansi -lpcre -lfuse -lrlog
srcdir=src
builddir=build

all: $(builddir) loggedfs

$(builddir):
	mkdir $(builddir)

loggedfs: $(builddir)/loggedfs.o $(builddir)/Config.o
	$(CC) -o loggedfs $(builddir)/loggedfs.o $(builddir)/Config.o $(LDFLAGS)

$(builddir)/loggedfs.o: $(srcdir)/loggedfs.cpp
	$(CC) -o $(builddir)/loggedfs.o -c $(srcdir)/loggedfs.cpp $(CFLAGS)

$(builddir)/Config.o: $(srcdir)/Config.cpp $(srcdir)/Config.h
	$(CC) -o $(builddir)/Config.o -c $(srcdir)/Config.cpp $(CFLAGS)

clean:
	rm -rf $(builddir)

mrproper: clean
	rm loggedfs
	
release:
	tar -c --exclude="CVS" src/ config.cfg  Makefile | bzip2 - > loggedfs.tar.bz2