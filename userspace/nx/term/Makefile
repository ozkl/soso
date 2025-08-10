SOURCES := $(wildcard ./*.c)
OBJS    := $(SOURCES:.c=.o)

CC      := musl-clang
LD      := musl-clang
CFLAGS  := -c -O3 -I../../../../microwindows/src/include -I../../../TinyGL/include
LDFLAGS := -L../../../../microwindows/src/lib -L../../../TinyGL/src -lTinyGL -lnano-X

BIN_DIR := ../../bin
EXE     ?= term2                 # set with `make EXE=myprog`
TARGET  := $(BIN_DIR)/$(EXE)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(LD) $(OBJS) $(LDFLAGS) -o $@

$(BIN_DIR):
	mkdir -p $@

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f *.o $(TARGET)
