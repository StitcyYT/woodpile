#ifndef X8_CURSOR_H
#define X8_CURSOR_H

#include "x8.h"

#define X8_CURSOR_MAX_W      32
#define X8_CURSOR_MAX_H      32
#define X8_CURSOR_MAX_FRAMES 24
#define X8_CURSOR_PIXELS     (X8_CURSOR_MAX_W * X8_CURSOR_MAX_H)

typedef struct {
    int count;
    int w, h;
    int hot_x[X8_CURSOR_MAX_FRAMES];
    int hot_y[X8_CURSOR_MAX_FRAMES];
    unsigned int color[X8_CURSOR_MAX_FRAMES][X8_CURSOR_PIXELS];
    unsigned char alpha[X8_CURSOR_MAX_FRAMES][X8_CURSOR_PIXELS];
    int delay_ms;
    int frame;
} x8_cursor_t;

/* Load a CUR or ANI file from the VFS. Returns 0 on success. */
int x8_cursor_load(x8_cursor_t *cur, const char *path);

/* Frame control. reset selects frame 0, animate advances on a timer. */
void x8_cursor_reset(x8_cursor_t *cur);
void x8_cursor_set_frame(x8_cursor_t *cur, int frame);
void x8_cursor_animate(x8_cursor_t *cur, uint64_t now_ms);

/* Draw with (x, y) as the hotspot position. Only one cursor is on screen;
 * drawing a new one restores the framebuffer bits saved by the previous draw. */
void x8_cursor_draw(x8_cursor_t *cur, int x, int y);

/* Draw a decoded CUR/ICO image without touching cursor save state.
 * scale=1 is native size; scale=4 maps a 16x16 ICO into a 64x64 launcher slot. */
void x8_image_draw(const x8_cursor_t *img, int x, int y, int scale);
void x8_cursor_hide(void);

int x8_cursor_hot_x(const x8_cursor_t *cur);
int x8_cursor_hot_y(const x8_cursor_t *cur);
int x8_cursor_frames(const x8_cursor_t *cur);

#endif
