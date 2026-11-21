#include "kbd.h"
#include "framebuffer.h"
#include "time.h"
#include "serial.h"

static char line[256];
static int line_len;
static int line_ready;
static int shifted;
static int ctrl;

#define RAW_CAP 128
static int raw_events[RAW_CAP];
static int raw_head, raw_tail;

static void raw_push(int event) {
    int next = (raw_tail + 1) % RAW_CAP;
    if (next == raw_head) return;
    raw_events[raw_tail] = event;
    raw_tail = next;
}

int kbd_scancode_nb(void) {
    if (raw_head == raw_tail) return -1;
    int event = raw_events[raw_head];
    raw_head = (raw_head + 1) % RAW_CAP;
    return event;
}

void kbd_flush(void) {
    raw_head = 0;
    raw_tail = 0;
}

static const char ascii[128] = {
    [0x02]='1', [0x03]='2', [0x04]='3', [0x05]='4', [0x06]='5',
    [0x07]='6', [0x08]='7', [0x09]='8', [0x0A]='9', [0x0B]='0',
    [0x0C]='-', [0x0D]='=', [0x0E]='\b', [0x0F]='\t',

    [0x10]='q', [0x11]='w', [0x12]='e', [0x13]='r', [0x14]='t',
    [0x15]='y', [0x16]='u', [0x17]='i', [0x18]='o', [0x19]='p',
    [0x1A]='[', [0x1B]=']', [0x1C]='\n',

    [0x1E]='a', [0x1F]='s', [0x20]='d', [0x21]='f', [0x22]='g',
    [0x23]='h', [0x24]='j', [0x25]='k', [0x26]='l', [0x27]=';',
    [0x28]='\'', [0x2B]='\\',

    [0x2C]='z', [0x2D]='x', [0x2E]='c', [0x2F]='v', [0x30]='b',
    [0x31]='n', [0x32]='m', [0x33]=',', [0x34]='.', [0x35]='/',

    [0x39]=' ',
};

static char apply_shift(char c) {
    if (!shifted) return c;
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

static inline unsigned char inb(unsigned short port) {
    unsigned char v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void kbd_init(void) {
    line_len = 0;
    line_ready = 0;
    shifted = 0;
    ctrl = 0;
    raw_head = 0;
    raw_tail = 0;
    /* Drain anything left in the controller buffer. */
    while (inb(0x64) & 0x01) (void)inb(0x60);
}

void kbd_handler(void) {
    unsigned char sc = inb(0x60);
    outb(0x20, 0x20);

    if (sc == 0x2A || sc == 0x36) {
        shifted = 1;
        raw_push(sc | 0x200);
        return;
    }
    if (sc == 0xAA || sc == 0xB6) {
        shifted = 0;
        raw_push(sc | 0x100);
        return;
    }
    if (sc == 0x1D) ctrl = 1;
    if (sc == 0x9D) ctrl = 0;

    int event = sc | (shifted ? 0x200 : 0) | (ctrl ? 0x400 : 0) |
                ((sc & 0x80) ? 0x100 : 0);
    raw_push(event);

    if (fb_gui_active()) return;
    if (sc & 0x80) return;              /* key release: not typed input */

    char c = apply_shift(ascii[sc & 0x7F]);
    if (!c) return;

    if (c == '\b') {
        if (line_len > 0 && !line_ready) {
            line_len--;
            fb_putc('\b');
            fb_putc(' ');
            fb_putc('\b');
            fb_flush();
        }
        return;
    }

    if (c == '\n') {
        line[line_len] = '\0';
        line_ready = 1;
        fb_putc('\n');
        fb_flush();
        return;
    }

    if (line_ready || line_len >= 255) return;
    line[line_len++] = c;
    fb_putc(c);
    fb_flush();
}

int kbd_getline(char *buf, int max) {
    line_len = 0;
    line_ready = 0;

    uint64_t last = time_ticks();
    for (;;) {
        if (line_ready) {
            int n = line_len;
            if (n >= max) n = max - 1;
            for (int i = 0; i < n; i++) buf[i] = line[i];
            buf[n] = '\0';
            line_ready = 0;
            line_len = 0;
            return n;
        }
        uint64_t now = time_ticks();
        if (now != last) {
            time_idle_tick();
            last = now;
        }
        __asm__ volatile ("hlt");
    }
}

int kbd_getc(void) {
    uint64_t last = time_ticks();
    for (;;) {
        if (line_ready) {
            int c = (unsigned char)line[0];
            if (line_len > 1) {
                for (int i = 0; i < line_len - 1; i++) line[i] = line[i + 1];
                line_len--;
            } else {
                line_len = 0;
                line_ready = 0;
            }
            return c;
        }
        uint64_t now = time_ticks();
        if (now != last) {
            time_idle_tick();
            last = now;
        }
        __asm__ volatile ("hlt");
    }
}
