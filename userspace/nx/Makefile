SOURCES_C=$(patsubst %.c,%.o,$(wildcard ./*.c))


CC=soso-clang
LD=ld
CFLAGS=-c -O3 -I../../root/include/nano-x
LDFLAGS=-T../link.ld -m elf_i386 ../../root/lib/crt1.o -L../../root/lib -lTinyGL -lnano-X -lc -L/usr/lib/llvm-10/lib/clang/10.0.0/lib/linux -lclang_rt.builtins-i386

OBJ = $(SOURCES_C)

all: $(OBJ)

clean:
	-rm *.o
	-rm -f ../bin/$*

%.o:%.c
	mkdir -p ../bin
	$(CC) $(CFLAGS) $< -o $@
	$(LD) $@ $(LDFLAGS) -o ../bin/$*
