SOURCES_C=sosousdk.o


CC=soso-clang
LD=ld
CFLAGS=-c
LDFLAGS=-m elf_i386 -static

OBJ = $(SOURCES_C)

all: $(OBJ) libsosousdk.a

.o:
	$(CC) $(CFLAGS) $< -o $@

libsosousdk.a: $(OBJ) ../font/font.o
	ar --target=elf32-i386 rcs $@ $^
	cp ../kernel/commonuser.h ../root/include/
	cp sosousdk.h ../root/include/
	cp libsosousdk.a ../root/lib/

clean:
	-rm *.o
	-rm *.a
	-rm ../root/lib/libsosousdk.a
	-rm ../root/include/sosousdk.h
	-rm ../root/include/commonuser.h
