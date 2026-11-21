#include "panic.h"
#include "framebuffer.h"
#include "serial.h"
#include "initramfs.h"

#define VGA_ADDR    ((volatile uint16_t *)0xB8000)
#define VGA_COLS    80
#define VGA_ROWS    25

#define LINE_MAX    128
#define RULE_LEN    88

/* 0 = no panic, 1 = full writer, 2 = severe writer, 3 = wallpaper writer.
 * Anything >= 2 is terminal: a fault inside those writers just halts. */
static volatile int panic_state;

static int str_len(const char *s) {
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static void append(char *buf, int *pos, int cap, const char *s) {
    if (!s) return;
    while (*s && *pos < cap - 1)
        buf[(*pos)++] = *s++;
    buf[*pos] = 0;
}

static void append_char(char *buf, int *pos, int cap, char c) {
    if (*pos < cap - 1)
        buf[(*pos)++] = c;
    buf[*pos] = 0;
}

static void fill_char(char *buf, int *pos, int cap, char c, int count) {
    while (count-- > 0 && *pos < cap - 1)
        buf[(*pos)++] = c;
    buf[*pos] = 0;
}

static void hex_fixed(char *out, uint64_t v, int digits) {
    static const char h[] = "0123456789ABCDEF";
    if (digits < 1 || digits > 16) digits = 16;
    out[digits] = 0;
    for (int i = digits - 1; i >= 0; i--) {
        out[i] = h[(v >> ((digits - 1 - i) * 4)) & 0xF];
    }
}

static void build_full_lines(const char *reason, uint32_t code, uint64_t dump,
                             char header[LINE_MAX],
                             char dump_line[LINE_MAX],
                             char reason_line[LINE_MAX],
                             char rule[LINE_MAX]) {
    (void)code;
    int n = 0;
    fill_char(rule, &n, LINE_MAX, '-', RULE_LEN);

    n = 0;
    append(header, &n, LINE_MAX, "kernel panic!");
    while (n < 24) append_char(header, &n, LINE_MAX, ' ');
    while (n < RULE_LEN) append_char(header, &n, LINE_MAX, '-');

    char hbuf[17];
    n = 0;
    append(dump_line, &n, LINE_MAX, "dump:0x");
    hex_fixed(hbuf, dump, 16);
    append(dump_line, &n, LINE_MAX, hbuf);

    n = 0;
    append(reason_line, &n, LINE_MAX, "reason: ");
    append(reason_line, &n, LINE_MAX, reason ? reason : "unknown");
}

static void build_severe_lines(const char *reason, uint32_t code, uint64_t dump,
                               char l1[LINE_MAX], char l2[LINE_MAX],
                               char l3[LINE_MAX], char l4[LINE_MAX],
                               char l5[LINE_MAX]) {
    int n = 0;
    append(l1, &n, LINE_MAX, "[!] kernel panic");

    char hbuf[17];
    n = 0;
    append(l2, &n, LINE_MAX, "dump:0x");
    hex_fixed(hbuf, dump, 16);
    append(l2, &n, LINE_MAX, hbuf);

    n = 0;
    append(l3, &n, LINE_MAX, "ah=0x");
    hex_fixed(hbuf, code & 0xFF, 2);
    append(l3, &n, LINE_MAX, hbuf);
    append(l3, &n, LINE_MAX, " --------");

    n = 0;
    append(l4, &n, LINE_MAX, "reason: ");
    append(l4, &n, LINE_MAX, reason ? reason : "unknown");

    n = 0;
    append(l5, &n, LINE_MAX, "kernel panic [!]");
}

static void serial_lines(char *a, char *b, char *c, char *d, char *e) {
    char *lines[5] = { a, b, c, d, e };
    for (int i = 0; i < 5; i++) {
        if (!lines[i] || !lines[i][0]) continue;
        serial_puts(lines[i]);
        serial_puts("\n");
    }
}

static void vga_putstr(int row, int col, const char *s) {
    if (row < 0 || row >= VGA_ROWS) return;
    for (int i = 0; s[i] && (col + i) < VGA_COLS; i++) {
        int c = col + i;
        if (c < 0) continue;
        VGA_ADDR[row * VGA_COLS + c] = 0x0F00 | (unsigned char)s[i];
    }
}

static void vga_clear(void) {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_ADDR[i] = 0x0F00 | ' ';
}

static void vga_full(char *rule, char *header, char *dump_line,
                     char *reason_line) {
    vga_clear();
    vga_putstr(0, 0, rule);
    vga_putstr(2, 0, header);
    vga_putstr(4, 0, dump_line);
    vga_putstr(5, 0, reason_line);
    vga_putstr(7, 0, rule);
}

static void vga_severe(char *l1, char *l2, char *l3, char *l4, char *l5) {
    vga_clear();
    vga_putstr(1, 0, l1);
    vga_putstr(3, 0, l2);
    vga_putstr(4, 0, l3);
    vga_putstr(5, 0, l4);
    vga_putstr(7, 0, l5);
}

static void fb_rule(int y, unsigned int dim, unsigned int bg,
                    const char *rule) {
    int cols = fb_width() / 8;
    int x = 8;
    while (cols > 0 && x < fb_width()) {
        int take = cols < RULE_LEN ? cols : RULE_LEN;
        char buf[LINE_MAX];
        int n = 0;
        while (n < take && n < LINE_MAX - 1)
            buf[n++] = rule[n];
        buf[n] = 0;
        fb_drawstr(x, y, buf, dim, bg);
        x += n * 8;
        cols -= n;
        if (n <= 0) break;
    }
}

static void fb_full(char *rule, char *header, char *dump_line,
                    char *reason_line) {
    unsigned int bg = fb_color(0x00, 0x00, 0x00);
    unsigned int fg = fb_color(0xFF, 0xFF, 0xFF);
    unsigned int dim = fb_color(0x80, 0x80, 0x80);
    unsigned int red = fb_color(0xFF, 0x50, 0x50);

    fb_clear(bg);
    fb_rule(16, dim, bg, rule);
    fb_drawstr(8, 48, header, red, bg);
    fb_drawstr(8, 80, dump_line, fg, bg);
    fb_drawstr(8, 104, reason_line, fg, bg);
    fb_rule(136, dim, bg, rule);
}

static void fb_severe(char *l1, char *l2, char *l3, char *l4, char *l5) {
    /* Minimal overlay: no fullscreen clear, just enough text to be seen if
     * the framebuffer still works.  Serial output is already on its way. */
    unsigned int bg = fb_color(0x00, 0x00, 0x00);
    unsigned int fg = fb_color(0xFF, 0xFF, 0xFF);
    unsigned int red = fb_color(0xFF, 0x50, 0x50);

    fb_rect(0, 0, fb_width(), 6 * 16, bg);
    fb_drawstr(8, 8, l1, red, bg);
    fb_drawstr(8, 32, l2, fg, bg);
    fb_drawstr(8, 56, l3, fg, bg);
    fb_drawstr(8, 80, l4, fg, bg);
    fb_drawstr(8, 104, l5, red, bg);
}

static void halt_forever(void) {
    for (;;) __asm__ volatile ("hlt");
}

void panic_severe(const char *reason, uint32_t code, uint64_t dump) {
    __asm__ volatile ("cli");
    if (panic_state >= 2) halt_forever();
    panic_state = 2;

    char l1[LINE_MAX], l2[LINE_MAX], l3[LINE_MAX], l4[LINE_MAX], l5[LINE_MAX];
    build_severe_lines(reason, code, dump, l1, l2, l3, l4, l5);

    /* Serial first: even a broken framebuffer or VGA text buffer cannot hide
     * the panic from a QEMU/com0 capture. */
    serial_lines(l1, l2, l3, l4, l5);
    if (fb_usable())
        fb_severe(l1, l2, l3, l4, l5);
    vga_severe(l1, l2, l3, l4, l5);
    halt_forever();
}

void panic(const char *reason, uint32_t code, uint64_t dump) {
    __asm__ volatile ("cli");
    if (panic_state) {
        /* A fault inside the panic writer means the situation is severe:
         * drop to the tiny serial-first format and never touch video again. */
        panic_severe(reason, code, dump);
        return;
    }
    panic_state = 1;

    char rule[LINE_MAX], header[LINE_MAX], dump_line[LINE_MAX];
    char reason_line[LINE_MAX];
    build_full_lines(reason, code, dump, header, dump_line, reason_line, rule);

    serial_lines(rule, header, dump_line, reason_line, rule);
    if (fb_usable())
        fb_full(rule, header, dump_line, reason_line);
    else
        vga_full(rule, header, dump_line, reason_line);

    panic_state = 2;
    halt_forever();
}

static void emit_worst_serial(const char *reason, uint64_t dump) {
    char hbuf[17];
    serial_puts("[!!] kernel panic\n");
    serial_puts("dump:0x");
    hex_fixed(hbuf, dump, 16);
    serial_puts(hbuf);
    serial_puts("\nreason: ");
    serial_puts(reason ? reason : "unknown");
    serial_puts("\nwallpaper: assets/wpreload.raw\n");
    serial_puts("kernel panic [!!]\n");
}

void panic_worst(const char *reason, uint32_t code, uint64_t dump) {
    __asm__ volatile ("cli");
    if (panic_state >= 2) halt_forever();
    panic_state = 3;

    (void)code;
    emit_worst_serial(reason, dump);

    int drawn = 0;
    if (fb_usable()) {
        /* Read the raw blob straight from the initramfs: the worst panic
         * must not depend on VFS slots/data-pool limits being available. */
        initramfs_file_t *file = initramfs_find("assets/wpreload.raw");
        if (file && file->data && file->data_len >= 8) {
            const unsigned char *b = file->data;
            uint32_t size = file->data_len;
            uint32_t w = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
                         ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
            uint32_t h = (uint32_t)b[4] | ((uint32_t)b[5] << 8) |
                         ((uint32_t)b[6] << 16) | ((uint32_t)b[7] << 24);
            uint64_t need = 8ULL + (uint64_t)w * h * 3ULL;
            if (w > 0 && h > 0 && need <= size) {
                fb_blit_scaled_rgb(b + 8, (int)w, (int)h);
                drawn = 1;
            }
        }
    }

    if (!drawn) {
        char l1[LINE_MAX], l2[LINE_MAX], l3[LINE_MAX], l4[LINE_MAX], l5[LINE_MAX];
        build_severe_lines(reason, code, dump, l1, l2, l3, l4, l5);
        if (fb_usable())
            fb_severe(l1, l2, l3, l4, l5);
        vga_severe(l1, l2, l3, l4, l5);
    }
    halt_forever();
}

void panic_trigger(unsigned int level) {
    switch (level) {
        case 1:
            panic("page fault (triggered)", 0, 0x000000000FFFFFFFULL);
            break;
        case 2:
            panic_severe("kernel memory fault", 0x0E, 0x000000000FFFFFFFULL);
            break;
        case 3:
            panic_worst("sysrq rcrq (triggered)", 0, 0x000000000FFFFFFFULL);
            break;
        default:
            break;
    }
}
