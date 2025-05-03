SOURCES_C=$(patsubst %.c,%.o,$(wildcard ./*.c))


CC=soso-clang
LD=soso-clang
CFLAGS=-c -O3
LDFLAGS=

OBJ = $(SOURCES_C)

all: $(OBJ)

clean:
	-rm *.o
	-rm -rf bin

%.o:%.c
	mkdir -p bin
	$(CC) $(CFLAGS) $< -o $@
	$(LD) $@ $(LDFLAGS) -o bin/$*
