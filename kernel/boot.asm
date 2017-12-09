MBOOT_PAGE_ALIGN    equ 1<<0
MBOOT_MEM_INFO      equ 1<<1
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)


[BITS 32]


section .multiboot
align 4
    dd  MBOOT_HEADER_MAGIC
    dd  MBOOT_HEADER_FLAGS
    dd  MBOOT_CHECKSUM


section .bss
align 16
stack_bottom:
resb 16384 ; 16 KiB
stack_top:

[GLOBAL _start]  ; this is the entry point. we tell linker script to set start address of kernel elf file.
[EXTERN kmain]

section .text

_start:
    mov esp, stack_top

    ; push multiboot parameter to kmain()
    push ebx

    ; ...and run!
    cli
    call kmain

    ;never reach here
    cli
    hlt
