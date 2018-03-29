SOURCES_C=$(patsubst %.c,%.o,$(wildcard kernel/*.c))
SOURCES_ASM=$(patsubst %.asm,%.o,$(wildcard kernel/*.asm))


CC=cc
LD=ld
REDZONE=-mno-red-zone
CFLAGS=-nostdlib -nostdinc -fno-builtin -m32 -c
LDFLAGS=-Tlink.ld -m elf_i386
ASFLAGS=-felf

OBJ = $(SOURCES_ASM) $(SOURCES_C)

all: $(OBJ) link

clean:
	-rm kernel/*.o kernel.bin

link:
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJ) font/font.o

kernel/%.o:kernel/%.c
	$(CC) $(CFLAGS) $< -o $@

kernel/%.o:kernel/%.asm
	nasm $(ASFLAGS) $< -o $@
