#include "abi.h"
#include "framebuffer.h"
#include "vfs.h"
#include "xec.h"
#include "serial.h"
#include "time.h"
#include "kbd.h"
#include "mouse.h"
#include "audio.h"

static volatile uint64_t *const table = (volatile uint64_t *)WP_ABI_BASE;

static void put(int index, void *fn) {
    table[index] = (uint64_t)(unsigned long)fn;
}

void abi_init(void) {
    for (int i = 0; i < ABI_COUNT; i++) table[i] = 0;

    put(ABI_TTY_CLEAR,     (void *)&fb_console_clear);
    put(ABI_TTY_PUTC,      (void *)&fb_putc);
    put(ABI_TTY_PUTS,      (void *)&fb_puts);
    put(ABI_TTY_SETCOLOR,  (void *)&fb_setcolor);
    put(ABI_TTY_SETXY,     (void *)&fb_setxy);
    put(ABI_TTY_GETXY,     (void *)&fb_getxy);
    put(ABI_TTY_COLS,      (void *)&fb_cols);
    put(ABI_TTY_ROWS,      (void *)&fb_rows);
    put(ABI_TTY_SETCELL,   (void *)&fb_setcell);
    put(ABI_TTY_CLEARLINE, (void *)&fb_clearline);
    put(ABI_TTY_FLUSH,     (void *)&fb_flush);
    put(ABI_TTY_SCROLL,    (void *)&fb_scroll);

    put(ABI_FS_READ,       (void *)&vfs_read);
    put(ABI_FS_LIST,       (void *)&vfs_list);
    put(ABI_FS_REMOVE,     (void *)&vfs_remove);
    put(ABI_FS_WRITE,      (void *)&vfs_write);
    put(ABI_FS_ISDIR,      (void *)&vfs_is_dir);

    put(ABI_XEC_RUN,       (void *)&xec_run);

    put(ABI_SERIAL_PUTS,   (void *)&serial_puts);
    put(ABI_SERIAL_PUTHEX, (void *)&serial_puthex);
    put(ABI_TIME_SLEEP,    (void *)&time_sleep);
    put(ABI_TIME_TICKS,    (void *)&time_ticks);
    put(ABI_TIME_CPU,      (void *)&time_cpu_usage);
    put(ABI_TIME_UPTIME,   (void *)&time_uptime_ms);

    put(ABI_KBD_GETLINE,   (void *)&kbd_getline);
    put(ABI_KBD_GETC,      (void *)&kbd_getc);

    put(ABI_AUDIO_PROBE,   (void *)&audio_probe);
    put(ABI_AUDIO_PLAY,    (void *)&audio_play);

    put(ABI_GFX_ENTER,     (void *)&fb_gui_enter);
    put(ABI_GFX_LEAVE,     (void *)&fb_gui_leave);
    put(ABI_GFX_WIDTH,     (void *)&fb_width);
    put(ABI_GFX_HEIGHT,    (void *)&fb_height);
    put(ABI_GFX_RGB,       (void *)&fb_color);
    put(ABI_GFX_CLEAR,     (void *)&fb_clear);
    put(ABI_GFX_PIXEL,     (void *)&fb_putpixel);
    put(ABI_GFX_GETPIXEL,  (void *)&fb_getpixel);
    put(ABI_GFX_RECT,      (void *)&fb_rect);
    put(ABI_GFX_TEXT,      (void *)&fb_drawstr);

    put(ABI_MOUSE_STATE,   (void *)&mouse_state);
    put(ABI_KBD_SCANCODE,  (void *)&kbd_scancode_nb);

    put(ABI_TTY_CAPTURE_SET, (void *)&fb_capture_set);
    put(ABI_TTY_CAPTURE_LEN, (void *)&fb_capture_len);
}
