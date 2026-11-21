[bits 64]
extern kmain
extern bss_start
extern bss_end
global _start
_start:
    mov word [0xB8000], 0x0F58
    lea rdi, [rel bss_start]
    lea rcx, [rel bss_end]
    sub rcx, rdi
    xor eax, eax
    cld
    rep stosb
    call kmain
.hang:
    hlt
    jmp .hang
