# soso
Soso is a simple unix-like operating system written in Nasm assembly and mostly in C. It supports multiboot, so it is started by Grub.
It can be built using Nasm and Clang. You can use GCC instead of Clang, if you want.
Tested build environments are Linux, FreeBSD, and Windows 10 (Windows Subsystem for Linux).

Soso is a 32-bit x86 operating system and its features are
- Multitasking with processes and threads
- Memory Paging with 4MB pages
- Kernelspace (runs in ring0) and userspace (runs in ring3) are separated
- Virtual File System
- FAT32 filesystem using FatFs
- System calls
- Libc (Newlib is ported with only basic calls like open, read,..)
- Can run userspace ELF files

Paging is written for 4MB page support, since it is easier to implement. Downside of this is, each process has to use at least 4MB memory.

Soso has Libc, so existing applications depending only on a small part of Libc can easly be ported to Soso. I have managed to build and run Lua on Soso!

# building
To build just run:

    make

this will build only kernel (kernel.bin). 

# running
QEMU has multiboot implementation so you can try the kernel with QEMU without creating an image. Just run:

    qemu-system-i386 -kernel kernel.bin

![Soso](screenshots/soso1.png)

By creating an image, it is possible to put some userspace programs and also it is possible to run Soso on real hardware!

# creating an image

TODO
