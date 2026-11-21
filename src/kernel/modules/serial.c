#include "serial.h"

static int serial_present = 0;

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);

    /* Loopback probe: a machine without a UART at 0x3F8 won't
       echo the byte back, so we can avoid hanging later. */
    outb(0x3F8 + 4, 0x1E);
    outb(0x3F8 + 0, 0xAE);
    serial_present = (inb(0x3F8 + 0) == 0xAE) ? 1 : 0;
    outb(0x3F8 + 4, 0x0B);
}

void serial_putc(char c) {
    if (!serial_present) return;
    while (!(inb(0x3F8 + 5) & 0x20));
    outb(0x3F8, c);
}

void serial_puts(const char *s) {
    for (int i = 0; s[i]; i++) serial_putc(s[i]);
}

void serial_puthex(uint64_t v) {
    char hex[] = "0123456789ABCDEF";
    char buf[17];
    buf[16] = 0;
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex[v & 0xF];
        v >>= 4;
    }
    serial_puts(buf);
}
