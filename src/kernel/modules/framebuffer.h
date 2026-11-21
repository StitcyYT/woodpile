#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "vbe.h"

/* Colour indices shared with .xec modules. */
enum {
    C_BLACK = 0,
    C_WHITE,
    C_RED,
    C_GREEN,
    C_BLUE,
    C_CYAN,
    C_MAGENTA,
    C_YELLOW,
    C_GRAY,
    C_TEAL,
    C_RAINBOW
};

void framebuffer_init(struct vbe_result *vbe);
unsigned int fb_color(unsigned char r, unsigned char g, unsigned char b);
int fb_ready(void);
int fb_width(void);
int fb_height(void);
void fb_putpixel(int x, int y, unsigned int color);
unsigned int fb_getpixel(int x, int y);
void fb_putchar(int x, int y, char c, unsigned int fg, unsigned int bg);
void fb_drawstr(int x, int y, const char *s, unsigned int fg, unsigned int bg);
void fb_clear(unsigned int color);
void fb_rect(int x, int y, int w, int h, unsigned int color);
void fb_printk(const char *s);
void fb_cursor_blink(void);

/* X8 GUI sessions draw straight into the framebuffer. */
void fb_gui_enter(void);
void fb_gui_leave(void);
int fb_gui_active(void);

/* Console helpers built on the same framebuffer and font. */
int fb_cols(void);
int fb_rows(void);
void fb_setcolor(int fg, int bg);
void fb_setxy(int col, int row);
unsigned int fb_getxy(void);
void fb_putc(int c);
void fb_puts(const char *s);
void fb_setcell(int col, int row, int ch, int fg, int bg);
void fb_clearline(int row);
void fb_console_clear(void);
void fb_flush(void);
void fb_scroll(void);
void fb_capture_set(void *buf, unsigned int cap);
unsigned int fb_capture_len(void);

#endif
