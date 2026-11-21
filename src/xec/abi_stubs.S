.intel_syntax noprefix
.code64
.text

.macro STUB name, index
.global \name
.type \name, @function
\name:
    jmp qword ptr [0x800000 + 8*\index]
.size \name, .-\name
.endm

STUB tty_clear,        0
STUB tty_putc,         1
STUB tty_puts,         2
STUB tty_setcolor,     3
STUB tty_setxy,        4
STUB tty_getxy,        5
STUB tty_cols,         6
STUB tty_rows,         7
STUB tty_setcell,      8
STUB tty_clearline,    9
STUB tty_flush,       10
STUB tty_scroll,      11

STUB fs_read,         12
STUB fs_list,         13
STUB fs_remove,       14
STUB fs_write,        15
STUB fs_is_dir,       16

STUB xec_run,         17

STUB serial_puts,     18
STUB serial_puthex,   19
STUB time_sleep,      20
STUB time_ticks,      21
STUB time_cpu_usage,  22
STUB time_uptime_ms,  23

STUB kbd_getline,     24
STUB kbd_getc,        25

STUB audio_probe,     26
STUB audio_play,      27

STUB gfx_enter,       28
STUB gfx_leave,       29
STUB gfx_width,       30
STUB gfx_height,      31
STUB gfx_rgb,         32
STUB gfx_clear,       33
STUB gfx_pixel,       34
STUB gfx_getpixel,    35
STUB gfx_rect,        36
STUB gfx_text,        37

STUB mouse_state,     38
STUB kbd_scancode,    39

STUB tty_capture_set, 40
STUB tty_capture_len, 41
