#include "memtest.h"
#include "pmm.h"
#include "kalloc.h"
#include "panic.h"

#define VGA ((volatile unsigned short *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

static void vga_clear(void) {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA[i] = 0x0700 | ' ';
}

static void vga_put(int row, int col, const char *s, unsigned char attr) {
    for (int i = 0; s[i] && (col + i) < VGA_COLS; i++)
        VGA[row * VGA_COLS + col + i] = (attr << 8) | (unsigned char)s[i];
}

static void vga_putc(int row, int col, char c, unsigned char attr) {
    if (col < VGA_COLS)
        VGA[row * VGA_COLS + col] = (attr << 8) | (unsigned char)c;
}

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void u64_to_dec(char *out, uint64_t v) {
    char tmp[20];
    int i = 0;
    if (v == 0) { out[0] = '0'; out[1] = 0; return; }
    while (v) { tmp[i++] = '0' + (v % 10); v /= 10; }
    for (int j = 0; j < i; j++) out[j] = tmp[i - 1 - j];
    out[i] = 0;
}

void memtest(uint64_t mem_top) {
    vga_clear();

    vga_put(0, 2, "memtest 2026.08.20f", 0x0F);

    vga_put(2, 3, "kb alloc", 0x07);

    pmm_init(mem_top);
    kalloc_init();

    int ok = 1;

    void *p1 = kalloc(2048);
    if (p1) {
        vga_put(3, 5, "2048 bytes", 0x07);
    } else {
        vga_put(3, 5, "2048 bytes", 0x07);
        ok = 0;
    }

    void *p2 = kalloc(1024);
    if (p2) {
        vga_put(4, 5, "1024 bytes", 0x07);
    } else {
        vga_put(4, 5, "1024 bytes", 0x07);
        ok = 0;
    }

    if (ok) {
        kfree(p1);
        kfree(p2);
        vga_put(6, 2, "memtest done..", 0x0A);
    } else {
        if (p1) kfree(p1);
        if (p2) kfree(p2);
        panic("OOM (out of memory)", 0, 0);
    }
}
