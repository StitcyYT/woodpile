[bits 64]
section .rodata
global font_data_start
font_data_start:
    incbin "src/kernel/font.bin"
