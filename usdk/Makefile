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
	cp ../kernel/commonuser.h ../usr/i686-soso/include/
	cp sosousdk.h ../usr/i686-soso/include/
	cp libsosousdk.a ../usr/i686-soso/lib/

clean:
	-rm *.o
	-rm *.a
	-rm -rf bin
	-rm ../usr/i686-soso/lib/libsosousdk.a
	-rm ../usr/i686-soso/include/sosousdk.h
	-rm ../usr/i686-soso/include/commonuser.h
