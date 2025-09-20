# soso
Soso is a simple unix-like operating system written in Nasm assembly and mostly in C. It supports multiboot, so it is started by Grub.
It can be built using Nasm and Clang.
Tested build environments are Linux, FreeBSD.

As of version 0.4, executables are 32 bit static Linux binaries.

Soso is a 32-bit x86 operating system and its features are
- Runs simple statically built Linux binaries (from version 0.4)
- Multitasking with processes and threads
- Paging
- Higher half kernel (from version 0.4)
- Kernelspace (runs in ring0) and userspace (runs in ring3) are separated
- Virtual File System
- FAT32 filesystem using FatFs
- System calls
- Userspace programs as ELF files (32 bit static Linux ELF executables)
- mmap support
- Framebuffer graphics (userspace can access with mmap)
- Shared memory
- Serial port
- PS/2 mouse
- Unix sockets
- TTY driver


![Soso](screenshots/soso-v0.3.png)

# running

You can download a [CD image (ISO file)](https://github.com/ozkl/soso/releases/download/v0.3/soso.zip) from releases and try it in a PC emulator like QEMU. When it is started, you can run: "doom", "lua" in a terminal window.

To try Soso in QEMU, just run:

    qemu-system-i386 -cdrom soso.iso

To run doom from a terminal window:

    cd /initrd
    doom

# building
To build kernel just run:

    make

this will build only kernel (kernel.bin). 

## Building userspace
You don't need a special compiler! Just build 32 bit static executables for Linux

