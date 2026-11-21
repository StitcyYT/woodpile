[org 0x1000]
[bits 32]

%ifndef TOTAL_SECTORS
%define TOTAL_SECTORS 59
%endif
%ifndef KERN_SECTORS
%define KERN_SECTORS 58
%endif
%ifndef IRAMFS_DST
%define IRAMFS_DST 0x20000
%endif

stage2:
    cli

    ; Copy kernel from 0x10200 to 0x1200
    cld
    mov esi, 0x10200
    mov edi, 0x1200
    mov ecx, KERN_SECTORS * 128
    rep movsd

    ; The kernel loads the initramfs itself from disk via ATA PIO.

    ; Page tables live above the kernel image so a growing kernel cannot be
    ; clobbered by the bootloader's identity mappings.
    mov edi, 0x20000
    xor eax, eax
    mov ecx, 4096*3/4
    rep stosd

    mov dword [0x20000], 0x21003
    mov dword [0x21000], 0x22003
    mov dword [0x21018], 0x22003
    mov dword [0x22000], 0x000083
    mov dword [0x22008], 0x200083
    mov dword [0x22010], 0x400083
    mov dword [0x22018], 0x600083
    mov dword [0x22020], 0x800083
    mov dword [0x22028], 0xA00083
    mov dword [0x22030], 0xC00083
    mov dword [0x22038], 0xE00083
    ; extra 16 MB for large media buffers and .xec BSS
    mov dword [0x22040], 0x1000083
    mov dword [0x22048], 0x1200083
    mov dword [0x22050], 0x1400083
    mov dword [0x22058], 0x1600083

    mov eax, 0x20000
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64_desc]

    jmp 0x08:lm

; ---------------- 64-bit ----------------

[bits 64]

lm:
    mov rsp, 0x80000

    mov rax, 0x1200
    call rax

.hang:
    hlt
    jmp .hang

; ---------------- GDT ----------------

align 16
gdt64:
    dq 0
    dq 0x00209A0000000000
    dq 0x0000920000000000

gdt64_end:

gdt64_desc:
    dw gdt64_end - gdt64 - 1
    dd gdt64

times 512-($-$$) db 0