SOURCES_C=sosousdk.o


CC=cc
LD=ld
CFLAGS=-I../usr/i686-soso/include -nostdlib -fno-builtin -m32 -Werror -c
LDFLAGS=-m elf_i386 -static

OBJ = $(SOURCES_C)

all: $(OBJ) libsosousdk.a

.o:
	$(CC) $(CFLAGS) $< -o $@

libsosousdk.a: $(OBJ) ../font/font.o
	ar rcs $@ $^

clean:
	-rm *.o
	-rm *.a
	-rm -rf bin
