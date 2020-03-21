# Toolchain
To build userspace executables, we can use Clang cross compiler with sysroot config and gnu linker ld.

## Linker
Soso requires userspace processes to be loaded after virtual memory location 0x40000000.

So we either need to use a linker script or a cross linker to link executables.
Binutils is patched with TEXT_START_ADDR=0x40000000 to achieve that.
