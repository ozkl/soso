SOURCES_C=$(patsubst %.c,%.o,$(wildcard ./*.c))


CC=musl-clang
LD=musl-clang
CFLAGS=-c -O3
LDFLAGS=-static 

OBJ = $(SOURCES_C)

all: $(OBJ)

clean:
	-rm *.o
	-rm -rf bin

%.o:%.c
	mkdir -p bin
	-$(CC) $(CFLAGS) $< -o $@
	-$(LD) $@ $(LDFLAGS) -o bin/$*
