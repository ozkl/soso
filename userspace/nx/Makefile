SOURCES_C=$(patsubst %.c,%.o,$(wildcard ./*.c))


CC=musl-clang
LD=musl-clang
CFLAGS=-c -O3 -I../../../microwindows/src/include -I../../TinyGL/include
LDFLAGS=-L../../../microwindows/src/lib -L../../TinyGL/src -lTinyGL -lnano-X

OBJ = $(SOURCES_C)

all: $(OBJ)

clean:
	-rm *.o
	-rm -f ../bin/$*

%.o:%.c
	mkdir -p ../bin
	$(CC) $(CFLAGS) $< -o $@
	$(LD) $@ $(LDFLAGS) -o ../bin/$*
