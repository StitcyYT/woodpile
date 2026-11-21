#ifndef X8_WINAPI_H
#define X8_WINAPI_H

#include "x8.h"

#define X8_WF_CLOSE 0x01
#define X8_WF_MENU  0x02

/* Window hit-test results. */
#define X8_HIT_NONE    0
#define X8_HIT_FRAME   1
#define X8_HIT_CLOSE   2
#define X8_HIT_CLIENT  3

typedef struct {
    int x, y, w, h;
    const char *title;
    const char *menu;
    int flags;
} x8_window_t;

/* Classic two-tone palette shared by the window manager and apps. */
#define X8_WIN_BLACK   x8_color(0, 0, 0)
#define X8_WIN_WHITE   x8_color(255, 255, 255)
#define X8_WIN_FACE    x8_color(214, 214, 214)
#define X8_WIN_DARK    x8_color(92, 92, 92)
#define X8_WIN_SHADOW  x8_color(135, 135, 135)
#define X8_WIN_BLUE    x8_color(0, 0, 160)
#define X8_WIN_CYAN    x8_color(0, 128, 128)
#define X8_WIN_YELLOW  x8_color(232, 216, 80)
#define X8_WIN_GREEN   x8_color(0, 140, 40)
#define X8_WIN_RED     x8_color(190, 30, 30)

void x8_window_init(x8_window_t *win, int x, int y, int w, int h,
                    const char *title, const char *menu, int flags);
void x8_window_draw(const x8_window_t *win, int focused);
int x8_window_hit(const x8_window_t *win, int mx, int my);
void x8_window_client(const x8_window_t *win, x8_rect_t *client);

void x8_button(int x, int y, int w, int h, const char *label, int pressed);
int x8_button_hit(int x, int y, int w, int h, int mx, int my);
void x8_status_bar(int y, int h, const char *message);
void x8_checker(int x, int y, int w, int h, int cell);

#endif
