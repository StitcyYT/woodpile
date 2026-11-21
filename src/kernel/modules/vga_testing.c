#include "vga_testing.h"

static int cursor = 0;

void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void set_cursor(unsigned short pos) {
    outb(VGA_CTRL, 0x0F);
    outb(VGA_DATA, (unsigned char)(pos & 0xFF));
    outb(VGA_CTRL, 0x10);
    outb(VGA_DATA, (unsigned char)((pos >> 8) & 0xFF));
}

void printk(const char *s) {
    outb(VGA_CTRL, 0x0A);
    outb(VGA_DATA, (inb(VGA_DATA) & 0xE0) | 0x00);
    outb(VGA_CTRL, 0x0B);
    outb(VGA_DATA, (inb(VGA_DATA) & 0xE0) | 0x0F);

    while(*s) {
        VGA[cursor++] = (*s++) | (0x0F << 8);
    }
    set_cursor(cursor);
}