[GLOBAL read_eip]
read_eip:
    pop eax
    jmp eax

[GLOBAL disable_paging]
disable_paging:
    mov edx, cr0
    and edx, 0x7fffffff
    mov cr0, edx
    ret

[GLOBAL enable_paging]
enable_paging:
    mov edx, cr0
    or edx, 0x80000000
    mov cr0, edx
    ret
