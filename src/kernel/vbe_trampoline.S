.intel_syntax noprefix

.section .trampoline, "ax"
.global trampoline_start
.global trampoline_end
.global trampoline_size
.global trampoline_entry32
.global trampoline_gdtr
.global trampoline_idtr
.global vbe_mode_try
.global vbe_result_fb_addr
.global vbe_result_width
.global vbe_result_height
.global vbe_result_pitch
.global vbe_result_bpp
.global vbe_result_success
.global vbe_result_red_pos
.global vbe_result_green_pos
.global vbe_result_blue_pos
.global vbe_result_mem_top
.global vbe_trace
.global best_mode
.global best_pixels
.global vbe_request_width
.global vbe_request_height
.global vbe_exact
.global kernel_return_addr
.global kernel_cr3

trampoline_start:

# ============================================================
# Fixed-offset data area (relative to 0x7000 physical base)
# ============================================================
.align 16
trampoline_gdt:
    .quad 0x0000000000000000              # 0x00: NULL
    .quad 0x00AF9A000000FFFF              # 0x08: 64-bit code, L=1, D=0, G=1, P=1, DPL=0, S=1, Type=0xA
    .quad 0x00AF92000000FFFF              # 0x10: 64-bit data, G=1, P=1, DPL=0, S=1, Type=0x2
    .quad 0x00CF9A000000FFFF              # 0x18: 32-bit code, D=1, G=1, P=1, DPL=0, S=1, Type=0xA
    .quad 0x00CF92000000FFFF              # 0x20: 32-bit data, D=1, G=1, P=1, DPL=0, S=1, Type=0x2
    .quad 0x00009A000000FFFF              # 0x28: 16-bit code, D=0, P=1, DPL=0, S=1, Type=0xA
    .quad 0x000092000000FFFF              # 0x30: 16-bit data, D=0, P=1, DPL=0, S=1, Type=0x2
trampoline_gdt_end:

.align 8
trampoline_gdtr:
    .word (trampoline_gdt_end - trampoline_gdt - 1)
    .quad 0  # filled in by C code with VBE_TRAMPOLINE_PHYS

.align 8
trampoline_idtr:
    .word 0  # limit, set to 0x3ff by vbe_switch.S
    .quad 0  # base, set to 0 by vbe_switch.S

# VBE data area
.align 2
vbe_mode_try:
    .word 0x4141              # mode currently being probed
best_mode:
    .word 0xFFFF              # best 32bpp LFB mode found so far
.align 4
best_pixels:
    .quad 0                   # pixel count of the best mode
vbe_request_width:
    .word 1920                # resolution requested by the kernel
vbe_request_height:
    .word 1080                # resolution requested by the kernel
vbe_exact:
    .byte 0                   # 1 = requested resolution was found
.align 4
mode_list_ptr:
    .quad 0                   # linear address of the firmware mode list
mode_count:
    .word 0                   # safety cap for malformed mode lists
vbe_trace:
    .byte 0                   # debug: which stage failed/succeeded
vbe_result_fb_addr:
    .quad 0                   # framebuffer physical address
vbe_result_width:
    .word 0                   # width in pixels
vbe_result_height:
    .word 0                   # height in pixels
vbe_result_pitch:
    .word 0                   # bytes per scanline
vbe_result_bpp:
    .byte 0                   # bits per pixel
vbe_result_success:
    .byte 0                   # 1 = success
vbe_result_red_pos:
    .byte 0                   # red field position
vbe_result_green_pos:
    .byte 0                   # green field position
vbe_result_blue_pos:
    .byte 0                   # blue field position
.align 8
kernel_return_addr:
    .quad 0

# E820 scratch + result (top of usable RAM)
.align 2
e820_buf:
    .space 24
.align 8
vbe_result_mem_top:
    .quad 0                   # top of usable RAM, set by real-mode E820

# VBE info buffers
.align 256
vbe_ctrl_info:
    .space 512
vbe_mode_info:
    .space 256

# ============================================================
# Entry from 64-bit long mode
# The C code does:
#   - Disable interrupts
#   - Save kernel GDTR/IDTR
#   - Load trampoline GDTR
#   - ljmp SEL_CODE32:PHYS_7400
# ============================================================
.align 16
.org trampoline_start + 0x400
trampoline_entry32:

# === 32-bit protected mode (paging enabled, compatibility mode) ===
.code32
    # Set up 32-bit data segments
    mov ax, SEL_DATA32
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x18000   # stack in dead stage1 load buffer, far from kernel data

    # Disable paging before switching to real mode
    mov eax, cr0
    and eax, ~0x80000000      # clear PG bit
    mov cr0, eax

    # Now paging is disabled, in 32-bit protected mode
    # Far jump to 16-bit code segment (clears CS cache)
    ljmp SEL_CODE16, 0f

# === 16-bit real mode ===
.code16
0:
    # Set up real-mode segments
    mov ax, SEL_DATA16
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x8000

    # Enter real mode by clearing PE bit
    mov eax, cr0
    and eax, ~0x00000001       # clear PE bit
    mov cr0, eax

    # Far jump to flush CS cache and enter real mode
    .byte 0xEA
    .word 1f, 0x0000

1:
    # Now in true 16-bit real mode
    # Set up real-mode segments to 0
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    # Stack at linear 0x18000 (seg 0x1000) so BIOS pushes can't
    # reach the kernel image below 0x8000
    mov ax, 0x1000
    mov ss, ax
    mov sp, 0x8000

    # Enable A20 gate via legacy port (fast method)
    in al, 0x92
    or al, 2
    out 0x92, al

    # Detect the top of usable RAM with E820 (independent of VBE)
    call e820_detect

    # ============================================================
    # VBE: enumerate whatever the firmware offers and pick the
    # highest-resolution 32bpp linear framebuffer mode.
    # ============================================================

    # Step 1: Get VBE controller info to find the mode list
    mov ax, VBE_FUNC_CTRL_INFO
    mov di, offset vbe_ctrl_info
    int 0x10

    cmp ax, 0x004F
    jne .VbeNoCtrl

    # Verify the "VESA" signature
    cmp dword ptr [di], 0x41534556
    jne .VbeNoSig

    # Step 2: read the mode list pointer (seg:off at 0x10:0x0E)
    movzx eax, word ptr [di + 0x10]       # segment
    shl eax, 4
    movzx edx, word ptr [di + 0x0E]       # offset
    add eax, edx                          # linear address of the mode list
    mov dword ptr [mode_list_ptr], eax

    # No candidate yet
    mov dword ptr [best_pixels], 1
    mov dword ptr [best_pixels + 4], 0
    mov word ptr [best_mode], 0xFFFF
    mov byte ptr [vbe_exact], 0
    mov word ptr [mode_count], 512        # safety cap

scan_mode_loop:
    cmp word ptr [mode_count], 0
    je scan_done
    dec word ptr [mode_count]

    # Read the next 16-bit mode number from the list via ES:SI
    mov eax, dword ptr [mode_list_ptr]
    mov edx, eax
    shr edx, 4
    mov es, dx
    and eax, 0xF
    mov si, ax

    mov ax, word ptr es:[si]
    cmp ax, 0xFFFF
    je scan_done

    # ES holds the mode-list segment; restore 0 so the VBE calls
    # (which use ES:DI as their buffer pointer) target our buffers.
    push ax
    xor ax, ax
    mov es, ax
    pop ax
    mov word ptr ds:[vbe_mode_try], ax

    push ax
    mov cx, ax
    or cx, 0x4000                         # bit 14: request LFB info
    mov ax, VBE_FUNC_MODE_INFO
    mov di, offset vbe_mode_info
    int 0x10
    pop bx                                # BX = mode number
    cmp ax, 0x004F
    jne scan_next

    # A candidate mode: require supported, LFB, 32bpp, direct color
    mov di, offset vbe_mode_info
    test word ptr [di], 0x01              # bit 0 = mode supported
    jz scan_next
    test word ptr [di], 0x80              # bit 7 = linear framebuffer
    jz scan_next
    cmp byte ptr [di + 0x19], 32          # bits_per_pixel
    jne scan_next
    cmp byte ptr [di + 0x1B], 6           # memory_model = direct color
    jne scan_next

    # Keep only modes that fit the requested resolution. An exact match
    # has the largest pixel count that fits, so it always wins; otherwise
    # the largest fitting mode is the closest supported resolution.
    movzx ecx, word ptr [di + 0x12]       # x_resolution
    movzx esi, word ptr [di + 0x14]       # y_resolution
    cmp cx, word ptr [vbe_request_width]
    ja scan_next
    cmp si, word ptr [vbe_request_height]
    ja scan_next

    # Compare total pixels (x * y) against the current best
    mov eax, ecx
    mul esi                               # edx:eax = pixels
    cmp eax, dword ptr [best_pixels]
    jbe scan_next

    # New best mode: stash everything we need
    mov dword ptr [best_pixels], eax
    mov dword ptr [best_pixels + 4], 0
    mov word ptr [best_mode], bx

    mov word ptr ds:[vbe_result_width], cx
    mov word ptr ds:[vbe_result_height], si
    mov eax, dword ptr [di + 0x28]        # phys_base_ptr (LFB address)
    mov dword ptr ds:[vbe_result_fb_addr], eax
    mov ax, word ptr [di + 0x10]          # bytes_per_scanline
    mov word ptr ds:[vbe_result_pitch], ax
    mov al, byte ptr [di + 0x19]          # bits_per_pixel
    mov byte ptr ds:[vbe_result_bpp], al
    mov al, byte ptr [di + 0x20]          # red_field_position
    mov byte ptr ds:[vbe_result_red_pos], al
    mov al, byte ptr [di + 0x22]          # green_field_position
    mov byte ptr ds:[vbe_result_green_pos], al
    mov al, byte ptr [di + 0x24]          # blue_field_position
    mov byte ptr ds:[vbe_result_blue_pos], al

    cmp cx, word ptr [vbe_request_width]
    jne scan_next
    cmp si, word ptr [vbe_request_height]
    jne scan_next
    mov byte ptr ds:[vbe_exact], 1

scan_next:
    add dword ptr [mode_list_ptr], 2
    jmp scan_mode_loop

scan_done:
    # No candidate at all from the firmware list
    xor ax, ax
    mov es, ax
    mov ax, word ptr [best_mode]
    cmp ax, 0xFFFF
    jne .SetBest
    mov byte ptr [vbe_trace], 4
    jmp try_hardcoded_modes
.SetBest:
    # Enable the highest-resolution mode we found
    xor ax, ax
    mov es, ax

    mov cx, word ptr [best_mode]
    mov bx, cx
    or bx, 0x4000                         # set bit 14 for linear framebuffer
    mov ax, VBE_FUNC_SET_MODE
    int 0x10

    cmp ax, 0x004F
    jne .NoSetBest

    mov byte ptr ds:[vbe_result_success], 1
    mov byte ptr [vbe_trace], 6
    jmp vbe_done
.NoSetBest:
    mov byte ptr [vbe_trace], 3
    jmp try_hardcoded_modes

.VbeNoCtrl:
    mov byte ptr [vbe_trace], 1
    jmp try_hardcoded_modes
.VbeNoSig:
    mov byte ptr [vbe_trace], 2
    jmp try_hardcoded_modes

try_hardcoded_modes:
    # Fallback: try a fixed, widely supported list if the firmware
    # mode list was missing or the selected mode could not be set.
    # Re-select against the requested resolution in case the list scan
    # had already chosen something.
    mov dword ptr [best_pixels], 1
    mov dword ptr [best_pixels + 4], 0
    mov word ptr [best_mode], 0xFFFF
    mov byte ptr [vbe_exact], 0

    mov si, offset mode_list
    mov cx, NUM_MODES

next_mode:
    lodsw                          # ax = mode number
    cmp ax, 0xFFFF
    je vbe_fail

    mov word ptr ds:[vbe_mode_try], ax

    push cx
    push si

    # Get VBE mode info
    mov cx, ax                     # CX = mode number
    or cx, 0x4000                  # bit 14: request LFB info (real BIOSes need this)
    mov ax, VBE_FUNC_MODE_INFO
    mov di, offset vbe_mode_info
    int 0x10

    pop si
    pop cx

    cmp ax, 0x004F
    jne next_mode_continue

    # Check if mode is 32-bit and has linear framebuffer
    mov di, offset vbe_mode_info
    test word ptr [di], 0x01       # bit 0 = mode supported
    jz next_mode_continue

    test word ptr [di], 0x80       # bit 7 = linear framebuffer
    jz next_mode_continue

    cmp byte ptr [di + 0x19], 32   # bits_per_pixel
    jne next_mode_continue

    # Check that memory model is direct color (0x06) or TTY (0x00)
    cmp byte ptr [di + 0x1B], 6    # memory_model = direct color
    je mode_found
    cmp byte ptr [di + 0x1B], 0    # memory_model = TTY/text (some BIOS use this)
    jne next_mode_continue

mode_found:
    # Reject anything larger than the requested resolution. The list is
    # ordered largest-first, so the first fitting mode is the closest one.
    mov bp, word ptr [di + 0x12]   # x_resolution
    mov dx, word ptr [di + 0x14]   # y_resolution
    cmp bp, word ptr [vbe_request_width]
    ja next_mode_continue
    cmp dx, word ptr [vbe_request_height]
    ja next_mode_continue

    # We found a suitable mode. Save results.
    mov word ptr ds:[vbe_result_width], bp
    mov word ptr ds:[vbe_result_height], dx
    mov eax, dword ptr [di + 0x28] # phys_base_ptr (linear framebuffer address)
    mov dword ptr ds:[vbe_result_fb_addr], eax
    mov ax, word ptr [di + 0x10]   # bytes_per_scanline
    mov word ptr ds:[vbe_result_pitch], ax
    mov al, byte ptr [di + 0x19]   # bits_per_pixel
    mov byte ptr ds:[vbe_result_bpp], al

    # Read channel field positions from the mode info block
    mov al, byte ptr [di + 0x20]   # red_field_position
    mov byte ptr ds:[vbe_result_red_pos], al
    mov al, byte ptr [di + 0x22]   # green_field_position
    mov byte ptr ds:[vbe_result_green_pos], al
    mov al, byte ptr [di + 0x24]   # blue_field_position
    mov byte ptr ds:[vbe_result_blue_pos], al

    mov bx, word ptr ds:[vbe_mode_try]
    mov word ptr [best_mode], bx

    cmp bp, word ptr [vbe_request_width]
    jne .fallback_not_exact
    cmp dx, word ptr [vbe_request_height]
    jne .fallback_not_exact
    mov byte ptr ds:[vbe_exact], 1
.fallback_not_exact:

    # Set the mode
    mov ax, VBE_FUNC_SET_MODE
    mov bx, word ptr ds:[vbe_mode_try]  # use actual mode from iteration
    or bx, 0x4000                  # set bit 14 for linear framebuffer
    int 0x10

    cmp ax, 0x004F
    jne next_mode_continue

    # Success!
    mov byte ptr ds:[vbe_result_success], 1
    mov byte ptr [vbe_trace], 7
    jmp vbe_done

next_mode_continue:
    dec cx
    jz vbe_fail
    jmp next_mode

vbe_fail:
    # No VBE mode found. Set a dummy flag.
    mov byte ptr ds:[vbe_result_success], 0
    mov byte ptr [vbe_trace], 5

vbe_done:
    # ============================================================
    # Return to protected mode
    # ============================================================
    # Load the trampoline GDT
    lgdt [trampoline_gdtr]

    # Enable protected mode
    mov eax, cr0
    or eax, 1                      # set PE bit
    mov cr0, eax

    # Far jump to 32-bit code segment
    .byte 0xEA
    .word ret32, SEL_CODE32

# ============================================================
# e820_detect: scan the BIOS memory map and stash the top of
# usable RAM (< 4 GiB) in vbe_result_mem_top. Falls back to 0
# when the BIOS lacks E820; the kernel applies a 128 MiB default.
# ============================================================
e820_detect:
    mov dword ptr [vbe_result_mem_top], 0
    mov dword ptr [vbe_result_mem_top + 4], 0

    mov edx, 0x534D4150              # 'SMAP'
    xor ebx, ebx                     # continuation value, 0 = first call
.MemNext:
    mov di, offset e820_buf
    mov eax, 0xE820
    mov ecx, 20
    int 0x15
    jc .MemDone                      # call failed
    cmp eax, 0x534D4150
    jne .MemDone                     # bogus signature
    cmp ecx, 20
    jb .MemDone                      # entry too small
    cmp dword ptr [di + 16], 1       # type 1 = usable RAM
    jne .MemCont
    cmp dword ptr [di + 4], 0        # base must be < 4 GiB
    jne .MemCont
    mov eax, dword ptr [di + 0]
    add eax, dword ptr [di + 8]      # end32 = base + length
    cmp dword ptr [di + 12], 0       # length must not cross 4 GiB
    jne .MemCont
    cmp eax, dword ptr [vbe_result_mem_top]
    jbe .MemCont
    mov dword ptr [vbe_result_mem_top], eax
.MemCont:
    test ebx, ebx
    jnz .MemNext                     # more entries to process
.MemDone:
    ret

.code32
ret32:
    # Now in 32-bit protected mode (paging disabled)
    mov ax, SEL_DATA32
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x18000   # same safe stack as entry

    # Enable paging and return to 64-bit mode
    # The kernel's CR3 and EFER.LME are still valid
    # We just need to set CR0.PG=1 and jump to 64-bit code

    # Load kernel CR3 (stored at a known location)
    mov eax, dword ptr ds:[kernel_cr3]
    mov cr3, eax

    # Enable paging
    mov eax, cr0
    or eax, 0x80000000             # set PG bit
    mov cr0, eax

    # Now in 32-bit compatibility mode with paging enabled
    # Far jump to 64-bit code segment
    ljmp SEL_CODE64, 0f

.code64
0:
    # Now in 64-bit long mode
    # Read the return address from the data area
    mov rax, qword ptr [kernel_return_addr]
    jmp rax

# Data in the code section
.align 4
kernel_cr3:
    .quad 0  # filled in by C code

mode_list:
    .word 0x147    # 1920x1080 32bpp
    .word 0x145    # 1680x1050 32bpp
    .word 0x143    # 1440x900  32bpp
    .word 0x141    # 1280x800  32bpp
    .word 0x118    # 1024x768  32bpp
    .word 0x115    # 800x600   32bpp
    .word 0x112    # 640x480   32bpp
    .word 0xFFFF   # terminator
NUM_MODES = 7

trampoline_end:

# Compute size for C code to know how much to copy
trampoline_size:
    .quad (trampoline_end - trampoline_start)
