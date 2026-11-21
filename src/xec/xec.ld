ENTRY(xec_entry)

SECTIONS {
    . = XEC_BASE;

    .text : {
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
    }

    . = ALIGN(8);
    _xec_image_end = .;

    .bss (NOLOAD) : {
        *(.bss .bss.*)
        *(COMMON)
    }

    _xec_bss_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.note*)
        *(.comment)
    }
}
