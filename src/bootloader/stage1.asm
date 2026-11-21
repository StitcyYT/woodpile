[org 0x7C00]
[bits 16]

jmp start

boot_drive db 0x80
iso_mode   db 0
chs_spt    db 63
chs_heads  db 16
cur_addr   dd 0

CODE_SEG equ 0x08
DATA_SEG equ 0x10

%ifndef TOTAL_SECTORS
%define TOTAL_SECTORS 59
%endif
%ifndef IRAMFS_ADDR
%define IRAMFS_ADDR 0xA00000
%endif
%ifndef IRAMFS_LBA
%define IRAMFS_LBA 94
%endif
%ifndef IRAMFS_SECS
%define IRAMFS_SECS 6759
%endif

start:
	cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7000
	mov [boot_drive], dl

	call a20
	call check_iso
	lgdt [gdtdsc_32]
	; The El Torito spec packet contains the geometry SeaBIOS derives from
	; the first partition entry in our boot sector.
	mov al, [0x510]
	inc al				; spec packet stores heads-1
	mov [chs_heads], al
	mov al, [0x511]
	and al, 0x3F
	mov [chs_spt], al
	call load_boot
	cmp byte [iso_mode], 0
	jz .skip_irfs
	call load_initramfs_iso
.skip_irfs:

	cli
	mov eax, cr0
	or eax, 1
	mov cr0, eax
	jmp CODE_SEG:main_32

; --- detect El Torito CD boot via "get bootable spec packet" ---
check_iso:
	mov byte [0x500], 0x13
	mov si, 0x500
	mov dl, [boot_drive]
	mov ax, 0x4B01
	int 0x13
	; AH=4B01 can return success on hard disks with an empty packet.
	; A real El Torito packet has a nonzero boot media type.
	setnc al
	cmp byte [0x502], 0
	setnz dl
	and al, dl
	mov [iso_mode], al
	ret

; SeaBIOS rejects AH=42 on El Torito-emulated drives, so convert the LBA in
; [dap_lba] to the geometry supplied in the spec packet and use AH=02.
; [dap_count] sectors are read into ES:BX.
read_chs_chunk:
	mov ax, [dap_lba]
	xor dx, dx
	div byte [chs_spt]	; AL=track*heads, AH=sector-1
	inc ah
	mov cl, ah			; CL = sector
	xor ah, ah
	div byte [chs_heads]	; AL=cylinder, AH=head
	mov ch, al			; CH = cylinder low 8 bits
	; Cylinders stay below 256 in these images.
	mov dh, ah			; DH = head
	mov al, [dap_count]
	mov ah, 0x02
	mov dl, [boot_drive]
	int 0x13
	ret

; Copy one BIOS-loaded chunk from low memory (0x1000) to the high address in
; [cur_addr].  The high GDT descriptor is patched before entering protected
; mode; SeaBIOS never sees that segment.
copy_chunk_to_high:
	mov eax, [cur_addr]
	mov [gdt_high+2], ax
	shr eax, 16
	mov [gdt_high+4], al
	mov [gdt_high+7], ah
	mov eax, cr0
	or al, 1
	mov cr0, eax
	mov ax, DATA_SEG
	mov ds, ax
	mov ax, 0x18
	mov es, ax
	cld
	mov si, 0x1000
	xor di, di
	mov cx, [dap_count]
	shl cx, 7			; sectors * 128 dwords
	rep movsd
	mov eax, cr0
	and al, ~1
	mov cr0, eax
	xor ax, ax
	mov ds, ax
	mov es, ax
	ret

; Preload initramfs into high memory via BIOS CHS reads (ISO boot only).
; The kernel's ATA PIO path cannot read the ATAPI CD-ROM.
load_initramfs_iso:
	mov dword [dap_lba], IRAMFS_LBA
	mov dword [cur_addr], IRAMFS_ADDR
	mov di, IRAMFS_SECS
.loop:
	mov bx, 16
	cmp di, bx
	ja .setn
	mov bx, di
.setn:
	mov [dap_count], bx
	xor ax, ax
	mov es, ax
	mov bx, 0x1000
	call read_chs_chunk
	jc .fail
	push di
	call copy_chunk_to_high
	pop di
	mov ax, [dap_count]
	add [dap_lba], ax
	shl ax, 9
	add [cur_addr], ax
	adc word [cur_addr+2], 0
	sub di, [dap_count]
	jnz .loop
	ret
.fail:
	ret

; Load stage2 and the kernel above stage1; main_32 relocates stage2 to 0x1000.
load_boot:
	mov di, TOTAL_SECTORS
.chunk:
	mov bx, 16
	cmp di, bx
	ja .setn
	mov bx, di
.setn:
	mov [dap_count], bx
	cmp byte [iso_mode], 0
	jz .lba_read
	mov ax, [dap+6]
	mov es, ax
	mov bx, [dap+4]
	call read_chs_chunk
	jmp .read_done
.lba_read:
	mov dl, [boot_drive]
	mov ah, 0x42
	mov si, dap
	int 0x13
.read_done:
	jc .fail
	sub di, [dap_count]
	jz .done
	mov ax, [dap_count]
	add [dap_lba], ax
	shl ax, 9
	add [dap+4], ax
	jmp .chunk
.done:
	ret
.fail:
	jmp $

a20:
	in al, 0x92
	or al, 0x02
	out 0x92, al
	ret

[bits 32]
main_32:
	mov ax, DATA_SEG
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov esp, 0x90000
	cld
	mov esi, 0x10000
	mov edi, 0x1000
	mov ecx, 128
	rep movsd
	jmp 0x08:0x1000

gdtdsc_32:
	dw gdte_32 - gdt_32 - 1
	dd gdt_32

; SeaBIOS peeks at the first partition entry to set the emulated hard-disk
; geometry.  heads=15, spt=63 yields the expected 16-head/63-sector CHS.
times 0x1BE-($-$$) db 0
	db 0x80
	db 0x00, 0x01, 0x00
	db 0x83
	db 0x0F, 0x3F, 0xFF
	dd 0
	dd 0x00008000

; DAP and GDT live in the unused MBR partition slots 1-3; SeaBIOS only reads
; partition 0 for the emulated geometry.
dap:
	db 0x10
	db 0
dap_count: dw TOTAL_SECTORS
	dw 0
	dw 0x1000
dap_lba: dq 1

gdt_32:
	dq 0
	dq 0x00CF9A000000FFFF
	dq 0x00CF92000000FFFF
gdt_high: dq 0x00CF92000000FFFF	; data descriptor patched with high base
gdte_32:

times 510-($-$$) db 0
dw 0xAA55
