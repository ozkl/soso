[GLOBAL flush_gdt]

flush_gdt:
    mov eax, [esp+4] ;[esp+4] is the parametered passed
    lgdt [eax]

    mov ax, 0x10  ;0x10 is the offset to our data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush ;0x08 is the offset to our code segment
.flush:
    ret

[GLOBAL flush_idt]

flush_idt:
    mov eax, [esp+4] ;[esp+4] is the parametered passed
    lidt [eax]
    ret

[GLOBAL flush_tss]

flush_tss:
    mov ax, 0x2B ;index of the TSS structure is 0x28 (5*8) and OR'ing two bits in order to set RPL 3 and we get 0x2B

    ltr ax
    ret
