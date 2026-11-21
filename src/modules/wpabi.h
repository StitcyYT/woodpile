#ifndef WPABI_H
#define WPABI_H

#include <stdint.h>

/* Kernel exports available to .co modules. */
void fb_clear(unsigned int color);
void fb_putpixel(int x, int y, unsigned int color);
unsigned int fb_getpixel(int x, int y);
void fb_putchar(int x, int y, char c, unsigned int fg, unsigned int bg);
void fb_drawstr(int x, int y, const char *s, unsigned int fg, unsigned int bg);
unsigned int fb_color(unsigned char r, unsigned char g, unsigned char b);
int fb_width(void);
int fb_height(void);
void mouse_get(int *x, int *y);
int fs_read(const char *path, const void **data, uint32_t *size);
int module_run_path(const char *path);
void serial_puts(const char *s);
void serial_puthex(uint64_t v);
void time_sleep(int ms);

#endif
