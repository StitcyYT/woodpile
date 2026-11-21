#ifndef X8_H
#define X8_H

#include "xecabi.h"

/* Raw framebuffer colours are built through the VBE channel map. */
#define X8_RGB(r, g, b) gfx_rgb((unsigned char)(r), (unsigned char)(g), (unsigned char)(b))

/* x8_key() returns plain ASCII for typeable keys, -1 when no event is
 * waiting, and these values for non-printable keys. */
#define X8_KEY_UP     0x100
#define X8_KEY_DOWN   0x101
#define X8_KEY_LEFT   0x102
#define X8_KEY_RIGHT  0x103
#define X8_KEY_ESC    0x104
#define X8_KEY_ENTER  0x105
#define X8_KEY_BACK   0x106
#define X8_KEY_CTRL_S 0x107

typedef struct {
    int x, y, w, h;
} x8_rect_t;

unsigned int x8_color(int r, int g, int b);
void x8_fill(int x, int y, int w, int h, unsigned int color);
void x8_border(int x, int y, int w, int h, unsigned int color);
void x8_hline(int x, int y, int w, unsigned int color);
void x8_vline(int x, int y, int h, unsigned int color);
void x8_line(int x0, int y0, int x1, int y1, unsigned int color);
int x8_text_width(const char *s);
void x8_text(int x, int y, const char *s, unsigned int fg, unsigned int bg);
void x8_text_center(int x, int y, int w, const char *s,
                    unsigned int fg, unsigned int bg);

void x8_mouse(int *x, int *y, int *buttons);
int x8_key(void);
void x8_flush_keys(void);
void x8_sleep(int ms);

int x8_inside(int x, int y, int rx, int ry, int rw, int rh);
void x8_swap(int *a, int *b);

#endif
