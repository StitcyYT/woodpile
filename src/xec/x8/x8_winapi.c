#include "x8_winapi.h"

void x8_window_init(x8_window_t *win, int x, int y, int w, int h,
                    const char *title, const char *menu, int flags) {
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->title = title ? title : "";
    win->menu = menu ? menu : "File  Options  Help";
    win->flags = flags;
}

void x8_window_client(const x8_window_t *win, x8_rect_t *client) {
    int top = (win->flags & X8_WF_MENU) ? 49 : 28;
    client->x = win->x + 3;
    client->y = win->y + top;
    client->w = win->w - 6;
    client->h = win->h - top - 3;
    if (client->w < 0) client->w = 0;
    if (client->h < 0) client->h = 0;
}

static void draw_close_box(int x, int y, int focused) {
    unsigned int face = focused ? X8_WIN_WHITE : X8_WIN_FACE;
    x8_fill(x, y, 14, 14, X8_WIN_BLACK);
    x8_fill(x + 1, y + 1, 12, 12, face);
    x8_vline(x + 1, y + 1, 12, X8_WIN_WHITE);
    x8_hline(x + 1, y + 1, 12, X8_WIN_WHITE);
    x8_vline(x + 12, y + 2, 11, X8_WIN_DARK);
    x8_hline(x + 2, y + 12, 11, X8_WIN_DARK);
    x8_line(x + 4, y + 4, x + 9, y + 9, X8_WIN_BLACK);
    x8_line(x + 9, y + 4, x + 4, y + 9, X8_WIN_BLACK);
}

void x8_window_draw(const x8_window_t *win, int focused) {
    x8_rect_t c;
    x8_window_client(win, &c);

    x8_fill(win->x, win->y, win->w, win->h, X8_WIN_FACE);
    x8_border(win->x, win->y, win->w, win->h, X8_WIN_BLACK);
    x8_hline(win->x + 1, win->y + 1, win->w - 2, X8_WIN_WHITE);
    x8_vline(win->x + 1, win->y + 1, win->h - 2, X8_WIN_WHITE);
    x8_vline(win->x + win->w - 2, win->y + 1, win->h - 2, X8_WIN_DARK);
    x8_hline(win->x + 1, win->y + win->h - 2, win->w - 2, X8_WIN_DARK);

    unsigned int bar = focused ? X8_WIN_BLUE : X8_WIN_DARK;
    unsigned int bar_text = focused ? X8_WIN_WHITE : X8_WIN_FACE;
    x8_fill(win->x + 3, win->y + 3, win->w - 6, 22, bar);
    x8_text(win->x + 26, win->y + 7, win->title, bar_text, bar);
    if (win->flags & X8_WF_CLOSE)
        draw_close_box(win->x + 6, win->y + 6, focused);

    if (win->flags & X8_WF_MENU) {
        x8_fill(win->x + 3, win->y + 26, win->w - 6, 20, X8_WIN_WHITE);
        x8_text(win->x + 9, win->y + 30, win->menu, X8_WIN_BLACK, X8_WIN_WHITE);
        x8_hline(win->x + 3, win->y + 46, win->w - 6, X8_WIN_SHADOW);
    }

    x8_fill(c.x, c.y, c.w, c.h, X8_WIN_WHITE);
    x8_border(c.x, c.y, c.w, c.h, X8_WIN_DARK);
}

int x8_window_hit(const x8_window_t *win, int mx, int my) {
    if (!x8_inside(mx, my, win->x, win->y, win->w, win->h))
        return X8_HIT_NONE;

    if ((win->flags & X8_WF_CLOSE) &&
        x8_inside(mx, my, win->x + 6, win->y + 6, 14, 14))
        return X8_HIT_CLOSE;

    x8_rect_t c;
    x8_window_client(win, &c);
    if (x8_inside(mx, my, c.x, c.y, c.w, c.h))
        return X8_HIT_CLIENT;

    return X8_HIT_FRAME;
}

void x8_button(int x, int y, int w, int h, const char *label, int pressed) {
    int ox = pressed ? 1 : 0;
    int oy = pressed ? 1 : 0;

    x8_fill(x, y, w, h, X8_WIN_BLACK);
    x8_fill(x + 1, y + 1, w - 2, h - 2, X8_WIN_FACE);
    x8_hline(x + 2 + ox, y + 2 + oy, w - 4, X8_WIN_WHITE);
    x8_vline(x + 2 + ox, y + 2 + oy, h - 4, X8_WIN_WHITE);
    x8_vline(x + w - 3 + ox, y + 3 + oy, h - 5, X8_WIN_DARK);
    x8_hline(x + 3 + ox, y + h - 3 + oy, w - 5, X8_WIN_DARK);
    x8_text_center(x + 2 + ox, y + (h - 16) / 2 + oy, w - 4, label,
                   X8_WIN_BLACK, X8_WIN_FACE);
}

int x8_button_hit(int x, int y, int w, int h, int mx, int my) {
    return x8_inside(mx, my, x, y, w, h);
}

void x8_status_bar(int y, int h, const char *message) {
    x8_fill(0, y, gfx_width(), h, X8_WIN_WHITE);
    x8_hline(0, y, gfx_width(), X8_WIN_BLACK);
    x8_hline(0, y + h - 1, gfx_width(), X8_WIN_SHADOW);
    x8_text(8, y + (h - 16) / 2, message, X8_WIN_BLACK, X8_WIN_WHITE);
}

void x8_checker(int x, int y, int w, int h, int cell) {
    if (cell <= 0) return;
    unsigned int a = x8_color(10, 58, 96);
    unsigned int b = x8_color(14, 74, 118);
    for (int row = 0; row < h; row += cell) {
        for (int col = 0; col < w; col += cell) {
            int odd = ((row / cell) + (col / cell)) & 1;
            int cw = col + cell <= w ? cell : w - col;
            int ch = row + cell <= h ? cell : h - row;
            x8_fill(x + col, y + row, cw, ch, odd ? b : a);
        }
    }
}
