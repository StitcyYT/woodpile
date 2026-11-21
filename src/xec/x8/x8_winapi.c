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

    if (x8_inside(mx, my, win->x + 3, win->y + 3, win->w - 6, 22))
        return X8_HIT_TITLE;

    x8_rect_t c;
    x8_window_client(win, &c);
    if (x8_inside(mx, my, c.x, c.y, c.w, c.h))
        return X8_HIT_CLIENT;

    return X8_HIT_FRAME;
}

/* --- smooth window dragging -------------------------------------------
 * While the mouse button is down we only move a 1px outline.  Saving the
 * pixels under that outline makes erase/reposition cheap, so no app content
 * or background is repainted until the button is released. */
#define X8_OUTLINE_MAX 8192
static unsigned short outline_x[X8_OUTLINE_MAX];
static unsigned short outline_y[X8_OUTLINE_MAX];
static unsigned int outline_color[X8_OUTLINE_MAX];
static int outline_count;
static int outline_on;
static int outline_x0, outline_y0, outline_w, outline_h;

static void outline_erase(void) {
    if (!outline_on) return;
    for (int i = 0; i < outline_count; i++)
        gfx_pixel(outline_x[i], outline_y[i], outline_color[i]);
    outline_on = 0;
    outline_count = 0;
}

static void outline_add(int x, int y) {
    if (outline_count >= X8_OUTLINE_MAX) return;
    outline_x[outline_count] = (unsigned short)x;
    outline_y[outline_count] = (unsigned short)y;
    outline_color[outline_count] = gfx_getpixel(x, y);
    outline_count++;
    gfx_pixel(x, y, X8_WIN_WHITE);
}

static void outline_draw(int x, int y, int w, int h) {
    outline_erase();
    if (w <= 0 || h <= 0) return;
    outline_x0 = x;
    outline_y0 = y;
    outline_w = w;
    outline_h = h;
    for (int px = x; px < x + w; px++) {
        outline_add(px, y);
        outline_add(px, y + h - 1);
    }
    for (int py = y + 1; py < y + h - 1; py++) {
        outline_add(x, py);
        outline_add(x + w - 1, py);
    }
    outline_on = 1;
}

void x8_desktop_restore_rect(int x, int y, int w, int h) {
    int sw = gfx_width(), sh = gfx_height();
    int top = 24, bottom = sh - 28;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < top ? top : y;
    int x1 = x + w > sw ? sw : x + w;
    int y1 = y + h > bottom ? bottom : y + h;
    if (x1 <= x0 || y1 <= y0) return;

    unsigned int a = x8_color(10, 58, 96);
    unsigned int b = x8_color(14, 74, 118);
    int cell = 24;
    for (int row = (y0 / cell) * cell; row < y1; row += cell) {
        int cy0 = row < y0 ? y0 : row;
        int cy1 = row + cell < y1 ? row + cell : y1;
        for (int col = (x0 / cell) * cell; col < x1; col += cell) {
            int cx0 = col < x0 ? x0 : col;
            int cx1 = col + cell < x1 ? col + cell : x1;
            if (cx1 <= cx0 || cy1 <= cy0) continue;
            int odd = ((row / cell) + (col / cell)) & 1;
            x8_fill(cx0, cy0, cx1 - cx0, cy1 - cy0, odd ? b : a);
        }
    }
}

void x8_desktop_background(void) {
    int sw = gfx_width(), sh = gfx_height();
    x8_desktop_restore_rect(0, 24, sw, sh - 52);
    x8_fill(0, 0, sw, 24, X8_WIN_WHITE);
    x8_hline(0, 23, sw, X8_WIN_BLACK);
    x8_status_bar(sh - 28, 28,
                  "Click an icon to launch  -  Esc to exit X8  -  PS/2 mouse active");
}

void x8_drag_init(x8_drag_t *d) {
    outline_erase();
    d->active = 0;
    d->dragging = 0;
    d->dx = d->dy = 0;
    d->nx = d->ny = 0;
}

int x8_window_drag(x8_drag_t *d, x8_window_t *win,
                   int mx, int my, int buttons, int hit) {
    if (!(buttons & 1)) {
        if (d->active && d->dragging) {
            outline_erase();
            win->x = d->nx;
            win->y = d->ny;
            d->active = 0;
            d->dragging = 0;
            return 1;
        }
        d->active = 0;
        d->dragging = 0;
        return 0;
    }

    if (!d->active) {
        if (hit != X8_HIT_TITLE) return 0;
        d->active = 1;
        d->dragging = 1;
        d->dx = mx - win->x;
        d->dy = my - win->y;
        d->nx = win->x;
        d->ny = win->y;
        outline_draw(win->x, win->y, win->w, win->h);
        return 2;
    }

    int sw = gfx_width(), sh = gfx_height();
    int nx = mx - d->dx;
    int ny = my - d->dy;
    if (nx < 0) nx = 0;
    if (ny < 24) ny = 24;
    if (nx + win->w > sw) nx = sw - win->w;
    if (ny + win->h > sh - 28) ny = sh - 28 - win->h;
    if (nx < 0) nx = 0;
    if (ny < 24) ny = 24;

    if (nx != d->nx || ny != d->ny) {
        d->nx = nx;
        d->ny = ny;
        outline_draw(nx, ny, win->w, win->h);
    }
    return 2;
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
