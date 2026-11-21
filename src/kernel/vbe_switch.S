.intel_syntax noprefix

.text
.code64

.global vbe_enter_trampoline
vbe_enter_trampoline:
    cli

    mov qword ptr [saved_rsp], rsp

    lea rax, [rip + .Lreturn_here]
    mov qword ptr [kernel_return_addr], rax

    mov rax, cr3
    mov qword ptr [kernel_cr3], rax

    sidt [saved_idtr]

    mov word ptr [trampoline_gdtr + 2], TRAMPOLINE_PHYS
    mov dword ptr [trampoline_gdtr + 4], 0

    lea rax, [trampoline_gdtr]
    lgdt [rax]

    # Point the IDT back at the real-mode IVT for the BIOS int 0x10 calls
    mov word ptr [trampoline_idtr], 0x3ff
    mov qword ptr [trampoline_idtr + 2], 0
    lea rax, [trampoline_idtr]
    lidt [rax]

    lea rax, [trampoline_ljmp_dest]
    ljmp [rax]

.Lreturn_here:
    lidt [saved_idtr]
    mov rsp, qword ptr [saved_rsp]
    ret

.data
.align 8
saved_rsp:
    .quad 0

.align 8
saved_idtr:
    .quad 0
    .quad 0

.align 4
trampoline_ljmp_dest:
    .long trampoline_entry32
    .word 0x18
