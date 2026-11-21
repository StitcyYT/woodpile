#include "panic.h"
#include "framebuffer.h"

#define VGA_ADDR    ((volatile uint16_t *)0xB8000)
#define VGA_COLS    80
#define VGA_ROWS    25

#define BS_ART_COLS 8
#define BS_ART_ROWS 6

static const char *bs_art[BS_ART_ROWS] = {
    "      __",
    " _   / /",
    "(_) | | ",
    " _  | | ",
    "(_) | | ",
    "     \\_\\ ",
};

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void append(char *buf, int *i, const char *s) {
    while (*s) buf[(*i)++] = *s++;
}

static void u32_hex(char *out, uint32_t v) {
    const char *h = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++) {
        out[7 - i] = h[v & 0xF];
        v >>= 4;
    }
    out[8] = 0;
}

static void u64_hex(char *out, uint64_t v) {
    const char *h = "0123456789ABCDEF";
    for (int i = 0; i < 16; i++) {
        out[15 - i] = h[v & 0xF];
        v >>= 4;
    }
    out[16] = 0;
}

static void vga_putstr(int row, int col, const char *s) {
    for (int i = 0; s[i] && (col + i) < VGA_COLS; i++)
        VGA_ADDR[row * VGA_COLS + col + i] = 0x1F00 | (unsigned char)s[i];
}

static void vga_centered(int row, const char *s) {
    int col = (VGA_COLS - str_len(s)) / 2;
    if (col < 0) col = 0;
    vga_putstr(row, col, s);
}

static void vga_bluescreen(const char *name, uint32_t err, uint64_t rip) {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_ADDR[i] = 0x1F00 | ' ';

    for (int r = 0; r < BS_ART_ROWS; r++)
        vga_putstr(5 + r, (VGA_COLS - BS_ART_COLS) / 2, bs_art[r]);

    vga_centered(12, "KERNEL PANIC");

    char line[64];
    char hbuf[20];
    int i = 0;
    append(line, &i, "EXCEPTION: ");
    append(line, &i, name);
    line[i] = 0;
    vga_centered(14, line);

    i = 0;
    append(line, &i, "ERR=0x");
    u32_hex(hbuf, err);
    append(line, &i, hbuf);
    line[i] = 0;
    vga_centered(15, line);

    i = 0;
    append(line, &i, "RIP=0x");
    u64_hex(hbuf, rip);
    append(line, &i, hbuf);
    line[i] = 0;
    vga_centered(16, line);

    vga_centered(18, "SYSTEM HALTED");
}

static void fb_bluescreen(const char *name, uint32_t err, uint64_t rip) {
    unsigned int blue  = fb_color(0x00, 0x00, 0xAA);
    unsigned int white = fb_color(0xFF, 0xFF, 0xFF);
    int w = fb_width();
    int h = fb_height();

    fb_clear(blue);

    int art_x = (w - BS_ART_COLS * 8) / 2;
    if (art_x < 0) art_x = 0;
    int y = (h - (BS_ART_ROWS * 16 + 16 + 16 + 16 + 3 * 16)) / 2;
    if (y < 0) y = 0;

    for (int r = 0; r < BS_ART_ROWS; r++)
        fb_drawstr(art_x, y + r * 16, bs_art[r], white, blue);
    y += BS_ART_ROWS * 16 + 16;

    char line[64];
    char hbuf[20];
    int i = 0;
    append(line, &i, "KERNEL PANIC");
    line[i] = 0;
    fb_drawstr((w - i * 8) / 2, y, line, white, blue);
    y += 16 + 16;

    i = 0;
    append(line, &i, "EXCEPTION: ");
    append(line, &i, name);
    line[i] = 0;
    fb_drawstr((w - i * 8) / 2, y, line, white, blue);
    y += 16;

    i = 0;
    append(line, &i, "ERR=0x");
    u32_hex(hbuf, err);
    append(line, &i, hbuf);
    line[i] = 0;
    fb_drawstr((w - i * 8) / 2, y, line, white, blue);
    y += 16;

    i = 0;
    append(line, &i, "RIP=0x");
    u64_hex(hbuf, rip);
    append(line, &i, hbuf);
    line[i] = 0;
    fb_drawstr((w - i * 8) / 2, y, line, white, blue);
}

void panic(const char *exc_name, uint32_t err_code, uint64_t rip) {
    __asm__ volatile ("cli");
    if (fb_ready())
        fb_bluescreen(exc_name, err_code, rip);
    else
        vga_bluescreen(exc_name, err_code, rip);
    while (1) { __asm__ volatile ("hlt"); }
}
