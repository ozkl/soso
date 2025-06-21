[BITS 32]

global _start
global kernel_page_directory
extern kmain

MBOOT_PAGE_ALIGN    equ 1<<0
MBOOT_MEM_INFO      equ 1<<1
MBOOT_USE_GFX       equ 1<<2
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO | MBOOT_USE_GFX
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

; 3GB offset for translating physical to virtual addresses
KERNEL_VIRTUAL_BASE equ 0xC0000000
; Page directory idx of kernel's 4MB PTE
KERNEL_PAGE_NUMBER     equ (KERNEL_VIRTUAL_BASE >> 22)

STACK_SIZE          equ 0x1000 ; 4KB stack

section .data
align 0x1000
; This PDE identity-maps the first 4MB of 32-bit physical address space
; bit 7: PS - kernel page is 4MB
; bit 1: RW - kernel page is R/W
; bit 0: P  - kernel page is present
kernel_page_directory:
    ; This page directory entry identity-maps the first 4MB of the 32-bit physical address space.
    ; All bits are clear except the following (0x00000083):
    ; bit 7: PS The kernel page is 4MB.
    ; bit 1: RW The kernel page is read/write.
    ; bit 0: P  The kernel page is present.
    ; This entry must be here -- otherwise the kernel will crash immediately after paging is
    ; enabled because it can't fetch the next instruction! It's ok to unmap this page later.
    dd 0x00000083
    times (KERNEL_PAGE_NUMBER - 1) dd 0                 ; Pages before kernel space.
    ; This page directory entry defines a 4MB page containing the kernel.
    dd 0x00000083
    times (1024 - KERNEL_PAGE_NUMBER - 1) dd 0  ; Pages after the kernel image.


section .text
align 4
MultiBootHeader:
    dd  MBOOT_HEADER_MAGIC
    dd  MBOOT_HEADER_FLAGS
    dd  MBOOT_CHECKSUM
    dd 0x00000000 ; header_addr
    dd 0x00000000 ; load_addr
    dd 0x00000000 ; load_end_addr
    dd 0x00000000 ; bss_end_addr
    dd 0x00000000 ; entry_addr
    ; Graphics requests
    dd 0x00000000 ; 0 = linear graphics
    dd 1024
    dd 768
    dd 32

; reserve initial kernel stack space -- that's 16k.
STACKSIZE equ 0x4000

_start:
    mov ecx, (kernel_page_directory - KERNEL_VIRTUAL_BASE)
    mov cr3, ecx    ; Load page directory

    mov ecx, cr4
    or ecx, 0x00000010  ; Set PSE bit in CR4 to enable 4MB pages
    mov cr4, ecx

    mov ecx, cr0
    or ecx, 0x80000000  ; Set PG bit in CR0 to enable paging
    mov cr0, ecx

    ; EIP currently holds physical address, so we need a long jump to
    ; the correct virtual address to continue execution in kernel space
    lea ecx, [start_higher_half]
    jmp ecx     ; Absolute jump!!

start_higher_half:
    ; Unmap identity-mapped first 4MB of physical address space
    ; mov dword [kernel_page_directory], 0
    ; invlpg [0]

    mov esp, kernel_stack_top ; set up stack pointer
    push eax    ; push header magic
    add ebx, KERNEL_VIRTUAL_BASE    ; make multiboot header pointer virtual
    push ebx    ; push header pointer (TODO: hopefully this isn't at an addr > 4MB)
    cli         ; disable interrupts
    call kmain

    cli
.hang:
    hlt
    jmp .hang

section .bss
global kernel_stack_bottom
global kernel_stack_top

kernel_stack_bottom:
    resb STACK_SIZE     ; reserve 4KB for kernel stack
kernel_stack_top: