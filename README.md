# soso
Soso is a simple unix-like operating system written in Nasm assembly and mostly in C. It supports multiboot, so it is started by Grub.
It can be built using Nasm and Clang.
Tested build environments are Linux, FreeBSD.

Soso is a 32-bit x86 operating system and its features are
- Multitasking with processes and threads
- Paging
- Kernelspace (runs in ring0) and userspace (runs in ring3) are separated
- Virtual File System
- FAT32 filesystem using FatFs
- System calls
- Libc (Musl is ported with basic calls like open, read,..)
- Userspace programs as ELF files
- mmap support
- Framebuffer graphics (userspace can access with mmap)
- Shared memory
- Serial port
- PS/2 mouse

Soso has Libc, so existing applications depending only on a small part of Libc can easly be ported to Soso. I have managed to build and run Lua and Doom on Soso!

# running

You can download a [CD image (ISO file)](https://github.com/ozkl/soso/releases/download/v0.2/soso.iso.zip) from releases and try in a virtualization software like VirtualBox or in a PC emulator like QEMU. When it is started, you can run: "doom", "ls", "lua". Executables are in /initrd.

To try Soso in QEMU, just run:

    qemu-system-i386 -cdrom soso.iso

## Lua
![Soso](screenshots/soso-v0.2_1.png)


## Doom
To demonstrate multitasking, two doom processes running at the same time:

![Doom on Soso](screenshots/soso-doom.png)

# building
To build kernel just run:

    make

this will build only kernel (kernel.bin). 

Building userspace binaries will be documented later.

