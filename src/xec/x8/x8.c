#include "x8.h"

static const char ascii[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = '\b', [0x0F] = '\t',

    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',

    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';',
    [0x28] = '\'', [0x2B] = '\\',

    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',

    [0x39] = ' ',
};

static char apply_shift(char c, int shift) {
    if (!shift) return c;
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    switch (c) {
        case '1': return '!'; case '2': return '@'; case '3': return '#';
        case '4': return '$'; case '5': return '%'; case '6': return '^';
        case '7': return '&'; case '8': return '*'; case '9': return '(';
        case '0': return ')'; case '-': return '_'; case '=': return '+';
        case '[': return '{'; case ']': return '}'; case '\\': return '|';
        case ';': return ':'; case '\'': return '"'; case ',': return '<';
        case '.': return '>'; case '/': return '?'; case '`': return '~';
        default: return c;
    }
}

unsigned int x8_color(int r, int g, int b) {
    return X8_RGB(r, g, b);
}

void x8_fill(int x, int y, int w, int h, unsigned int color) {
    gfx_rect(x, y, w, h, color);
}

void x8_border(int x, int y, int w, int h, unsigned int color) {
    if (w <= 0 || h <= 0) return;
    x8_fill(x, y, w, 1, color);
    x8_fill(x, y + h - 1, w, 1, color);
    x8_fill(x, y, 1, h, color);
    x8_fill(x + w - 1, y, 1, h, color);
}

void x8_hline(int x, int y, int w, unsigned int color) {
    x8_fill(x, y, w, 1, color);
}

void x8_vline(int x, int y, int h, unsigned int color) {
    x8_fill(x, y, 1, h, color);
}

void x8_line(int x0, int y0, int x1, int y1, unsigned int color) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        gfx_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

int x8_text_width(const char *s) {
    return (int)str_len(s) * 8;
}

void x8_text(int x, int y, const char *s, unsigned int fg, unsigned int bg) {
    gfx_text(x, y, s, fg, bg);
}

void x8_text_center(int x, int y, int w, const char *s,
                    unsigned int fg, unsigned int bg) {
    int tw = x8_text_width(s);
    int tx = x + (w - tw) / 2;
    if (tx < x) tx = x;
    gfx_text(tx, y, s, fg, bg);
}

void x8_mouse(int *x, int *y, int *buttons) {
    mouse_state(x, y, buttons);
}

int x8_key(void) {
    int ev = kbd_scancode();
    if (ev < 0) return -1;
    if (ev & 0x100) return -1;              /* key release */

    int sc = ev & 0x7F;
    int shift = (ev & 0x200) ? 1 : 0;
    int ctrl = (ev & 0x400) ? 1 : 0;

    if (ctrl && sc == 0x1F) return X8_KEY_CTRL_S;
    if (sc == 0x2A || sc == 0x36 || sc == 0x1D) return -1;

    switch (sc) {
        case 0x01: return X8_KEY_ESC;
        case 0x0E: return X8_KEY_BACK;
        case 0x1C: return X8_KEY_ENTER;
        case 0x48: return X8_KEY_UP;
        case 0x50: return X8_KEY_DOWN;
        case 0x4B: return X8_KEY_LEFT;
        case 0x4D: return X8_KEY_RIGHT;
        default: break;
    }

    if (sc >= 128) return -1;
    char c = ascii[sc];
    if (!c) return -1;
    return (unsigned char)apply_shift(c, shift);
}

void x8_flush_keys(void) {
    while (kbd_scancode() >= 0) { }
}

void x8_sleep(int ms) {
    time_sleep(ms);
}

int x8_inside(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void x8_swap(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}
