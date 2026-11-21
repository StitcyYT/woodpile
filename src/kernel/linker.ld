ENTRY(_start)

SECTIONS {
    . = 0x1200;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    . = 0xC000;

    .trampoline : {
        *(.trampoline)
    }

    . = 0xC800;

    .initptr : {
        *(.initptr)
    }

    /* Keep the entire first MiB untouched: SeaBIOS keeps its extra stack
     * and 16-bit C segment there, and the VBE BIOS calls need them intact. */
    . = 0x100000;

    bss_start = .;

    .bss (NOLOAD) : {
        *(.bss)
    }

    bss_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.note*)
        *(.comment)
    }
}
