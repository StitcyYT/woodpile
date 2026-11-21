#ifndef X8_SHELL_H
#define X8_SHELL_H

#include "x8.h"
#include "x8_winapi.h"

enum {
    X8_ICON_SNAKE = 0,
    X8_ICON_PONG,
    X8_ICON_PAINT,
    X8_ICON_NOTE,
    X8_ICON_TERM,
    X8_ICON_COUNT
};

/* Small shared record so child apps can repaint the launcher exactly where
 * the session left it, even after the user dragged it. */
typedef struct {
    uint32_t magic;
    int x, y, w, h;
    int hover;
    int menu_open;
} x8_shell_shared_t;

#define X8_SHELL_SHARED_ADDR  0x7F0000u
#define X8_SHELL_SHARED_MAGIC 0x58385348u   /* "X8SH" */

#define X8_SHELL_SHARED ((volatile x8_shell_shared_t *)X8_SHELL_SHARED_ADDR)

void x8_shell_init(void);
void x8_shell_publish(const x8_window_t *win, int hover, int menu_open);
void x8_shell_layout(x8_window_t *win, x8_rect_t icons[X8_ICON_COUNT]);
void x8_shell_layout_icons(const x8_window_t *win,
                           x8_rect_t icons[X8_ICON_COUNT]);
void x8_shell_draw(int hover, int menu_open);
void x8_shell_draw_icon(int kind, const x8_rect_t *r, int hover);

#endif
