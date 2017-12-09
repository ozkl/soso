SOURCES_C=$(patsubst %.c,%.o,$(wildcard ./*.c))


CC=cc
LD=ld
CFLAGS=-I../usr/i686-soso/include -nostdlib -fno-builtin -m32 -Werror -c
LDFLAGS=-Tlink.ld -m elf_i386 ../usr/i686-soso/lib/crt0.o -L../usr/i686-soso/lib -lc -lg -lm -lnosys

OBJ = $(SOURCES_C)

all: $(OBJ)

clean:
	-rm *.o

%.o:%.c
	$(CC) $(CFLAGS) $< -o $@
	$(LD) $@ $(LDFLAGS) -o $*
