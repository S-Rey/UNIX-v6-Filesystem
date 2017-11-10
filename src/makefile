#CFLAGS += -DDEBUG
CC=gcc
CFLAGS += -std=c99 -pedantic -g -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion -Wunreachable-code
LDLIBS+= -lcrypto


TARGET = test-dirent test-file test-inodes shell fs test-bitmap

all: $(TARGET)

shell:  error.o test-dirent.o mount.o inode.o sector.o filev6.o sha.o direntv6.o shell.o bmblock.o

test-dirent: test-core.o error.o test-dirent.o mount.o inode.o sector.o filev6.o sha.o direntv6.o bmblock.o

test-file: test-core.o error.o test-file.o mount.o inode.o sector.o filev6.o sha.o bmblock.o

test-inodes: test-core.o error.o test-inodes.o mount.o inode.o sector.o filev6.o bmblock.o

test-bitmap: test-bitmap.o bmblock.o

fs.o: fs.c  
	$(COMPILE.c) -D_DEFAULT_SOURCE $$(pkg-config fuse --cflags) -o $@ -c $<

fs: fs.o mount.o bmblock.o direntv6.o filev6.o sector.o inode.o error.o 
	$(LINK.c) -o $@ $^ $(LDLIBS) $$(pkg-config fuse --libs)

clean:
	rm -f *.o
	rm -f $(TARGET)
	echo Clean done


