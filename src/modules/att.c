#include "wpabi.h"

/* Source bitmap is the untouched 32x32 PNG converted to RGBA. */
#define SRC_W 32
#define SRC_H 32

/* Render it at half size: only this changes on screen. */
#define OUT_W 16
#define OUT_H 16

static const unsigned char *cursor;
static uint32_t cursor_len;
static const void *cursor_raw;

/* Only the rendered area under the cursor is ever repainted. */
static unsigned int saved[OUT_W * OUT_H];
static int saved_valid;
static int last_x, last_y;

static void save_area(int x, int y) {
    for (int r = 0; r < OUT_H; r++)
        for (int c = 0; c < OUT_W; c++)
            saved[r * OUT_W + c] = fb_getpixel(x + c, y + r);
}

static void restore_area(int x, int y) {
    for (int r = 0; r < OUT_H; r++)
        for (int c = 0; c < OUT_W; c++)
            fb_putpixel(x + c, y + r, saved[r * OUT_W + c]);
}

static void draw_cursor(int mx, int my) {
    if (!cursor || cursor_len < SRC_W * SRC_H * 4) return;

    /* 2x2 alpha-weighted downsample of the original pixels. */
    for (int y = 0; y < OUT_H; y++) {
        for (int x = 0; x < OUT_W; x++) {
            unsigned long ar = 0, ag = 0, ab = 0, asum = 0;
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    const unsigned char *p =
                        cursor + ((y * 2 + dy) * SRC_W + (x * 2 + dx)) * 4;
                    unsigned a = p[3];
                    ar += (unsigned long)p[0] * a;
                    ag += (unsigned long)p[1] * a;
                    ab += (unsigned long)p[2] * a;
                    asum += a;
                }
            }
            if (asum == 0) continue;
            fb_putpixel(mx + x, my + y,
                        fb_color((unsigned char)(ar / asum),
                                 (unsigned char)(ag / asum),
                                 (unsigned char)(ab / asum)));
        }
    }
}

/* Cursor implementation: teal desktop, arrow follows the PS/2 mouse. */
void module_entry(void) {
    unsigned int teal = fb_color(0, 128, 128);
    unsigned int white = fb_color(255, 255, 255);

    fb_clear(teal);
    fb_drawstr(12, 12, "WOODPILE OS 64", white, teal);

    if (fs_read("assets/cursor.raw", &cursor_raw, &cursor_len) == 0)
        cursor = (const unsigned char *)cursor_raw;
    serial_puts("att.co: cursor bytes=");
    serial_puthex(cursor_len);
    serial_puts("\n");

    for (;;) {
        int x = 0, y = 0;
        const void *mraw;
        uint32_t mlen;
        if (fs_read("/devc/umouse", &mraw, &mlen) == 0 && mlen >= 8) {
            const uint32_t *pos = (const uint32_t *)mraw;
            x = (int)pos[0];
            y = (int)pos[1];
        }

        if (!saved_valid || x != last_x || y != last_y) {
            if (saved_valid) restore_area(last_x, last_y);
            save_area(x, y);
            draw_cursor(x, y);
            last_x = x;
            last_y = y;
            saved_valid = 1;
        }
        time_sleep(8);
    }
}
