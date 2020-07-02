SOURCES_C=$(patsubst %.c,%.o,$(wildcard ./*.c))


CC=soso-clang
LD=ld
CFLAGS=-c -O3
LDFLAGS=-T./link.ld -m elf_i386 ../root/lib/crt1.o -L../root/lib -lsosousdk -lTinyGL -lc -L/usr/lib/llvm-9/lib/clang/9.0.1/lib/linux -lclang_rt.builtins-i386

OBJ = $(SOURCES_C)

all: $(OBJ)

clean:
	-rm *.o
	-rm -rf bin

%.o:%.c
	mkdir -p bin
	$(CC) $(CFLAGS) $< -o $@
	$(LD) $@ $(LDFLAGS) -o bin/$*
