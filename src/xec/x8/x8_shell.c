#include "x8_shell.h"
#include "x8_cursor.h"

static x8_cursor_t app_icons[X8_ICON_COUNT];
static x8_window_t shell_win;
static int shell_ready;

static const char *icon_labels[X8_ICON_COUNT] = {
    "Snake", "Ping Pong", "Paint", "Note Pad", "Terminal"
};

static const char *icon_image_paths[X8_ICON_COUNT] = {
    "sys32/icons/snake.ico",
    "sys32/icons/pingpong.ico",
    "sys32/icons/paint.ico",
    "sys32/icons/notepad.ico",
    "sys32/icons/term.ico"
};

void x8_shell_layout(x8_window_t *win, x8_rect_t icons[X8_ICON_COUNT]) {
    int sw = gfx_width(), sh = gfx_height();
    int ww = sw < 1024 ? sw - 40 : 920;
    int wh = sh < 720 ? sh - 80 : 640;
    if (ww < 420) ww = 420;
    if (wh < 400) wh = 400;

    int wx = (sw - ww) / 2;
    int wy = (sh - wh) / 2;
    if (wy < 28) wy = 28;
    x8_window_init(win, wx, wy, ww, wh, "Program Manager",
                   "File  Options  Help", X8_WF_CLOSE | X8_WF_MENU);
    x8_shell_layout_icons(win, icons);
}

void x8_shell_layout_icons(const x8_window_t *win,
                            x8_rect_t icons[X8_ICON_COUNT]) {
    x8_rect_t c;
    x8_window_client(win, &c);
    int tw = 168, th = 136, gap_x = 24, gap_y = 18;
    int rows = (X8_ICON_COUNT + 1) / 2;
    int grid_w = tw * 2 + gap_x;
    int grid_h = th * rows + gap_y * (rows - 1);
    int ox = c.x + (c.w - grid_w) / 2;
    int oy = c.y + 42 + ((c.h - 42 - grid_h) / 2);
    if (oy < c.y + 34) oy = c.y + 34;

    for (int i = 0; i < X8_ICON_COUNT; i++) {
        icons[i].x = ox + (i % 2) * (tw + gap_x);
        icons[i].y = oy + (i / 2) * (th + gap_y);
        icons[i].w = tw;
        icons[i].h = th;
    }
}

void x8_shell_init(void) {
    if (shell_ready) return;

    for (int i = 0; i < X8_ICON_COUNT; i++)
        (void)x8_cursor_load(&app_icons[i], icon_image_paths[i]);

    x8_rect_t icons[X8_ICON_COUNT];
    volatile x8_shell_shared_t *shared = X8_SHELL_SHARED;
    if (shared->magic == X8_SHELL_SHARED_MAGIC) {
        x8_window_init(&shell_win, shared->x, shared->y,
                       shared->w, shared->h, "Program Manager",
                       "File  Options  Help", X8_WF_CLOSE | X8_WF_MENU);
    } else {
        x8_shell_layout(&shell_win, icons);
    }
    shell_ready = 1;
}

void x8_shell_publish(const x8_window_t *win, int hover, int menu_open) {
    volatile x8_shell_shared_t *shared = X8_SHELL_SHARED;
    shared->magic = X8_SHELL_SHARED_MAGIC;
    shared->x = win->x;
    shared->y = win->y;
    shared->w = win->w;
    shared->h = win->h;
    shared->hover = hover;
    shared->menu_open = menu_open;

    x8_window_init(&shell_win, win->x, win->y, win->w, win->h,
                   "Program Manager", "File  Options  Help",
                   X8_WF_CLOSE | X8_WF_MENU);
    shell_ready = 1;
}

void x8_shell_draw_icon(int kind, const x8_rect_t *r, int hover) {
    if (kind < 0 || kind >= X8_ICON_COUNT) return;

    unsigned int face = hover ? x8_color(220, 236, 250) : X8_WIN_WHITE;
    x8_fill(r->x, r->y, r->w, r->h, face);
    x8_border(r->x, r->y, r->w, r->h, hover ? X8_WIN_BLUE : X8_WIN_SHADOW);
    x8_hline(r->x + 1, r->y + 1, r->w - 2, X8_WIN_WHITE);
    x8_vline(r->x + 1, r->y + 1, 1, X8_WIN_WHITE);

    int px = r->x + (r->w - 64) / 2;
    int py = r->y + 12;

    if (app_icons[kind].count > 0) {
        x8_image_draw(&app_icons[kind], px, py, 4);
    } else if (kind == X8_ICON_SNAKE) {
        x8_fill(px + 4, py + 4, 56, 52, x8_color(12, 74, 22));
        x8_border(px + 4, py + 4, 56, 52, X8_WIN_BLACK);
        x8_fill(px + 14, py + 28, 12, 10, x8_color(40, 200, 50));
        x8_fill(px + 24, py + 28, 12, 10, x8_color(30, 170, 40));
        x8_fill(px + 34, py + 28, 12, 10, x8_color(30, 170, 40));
        x8_fill(px + 42, py + 20, 10, 10, x8_color(50, 220, 60));
        x8_fill(px + 47, py + 23, 2, 2, X8_WIN_BLACK);
        x8_fill(px + 16, py + 10, 8, 8, X8_WIN_RED);
    } else if (kind == X8_ICON_PONG) {
        x8_fill(px + 4, py + 4, 56, 52, x8_color(0, 30, 90));
        x8_border(px + 4, py + 4, 56, 52, X8_WIN_WHITE);
        for (int y = py + 8; y < py + 52; y += 10)
            x8_fill(px + 31, y, 2, 5, X8_WIN_WHITE);
        x8_fill(px + 8, py + 16, 5, 22, X8_WIN_WHITE);
        x8_fill(px + 51, py + 24, 5, 22, X8_WIN_WHITE);
        x8_fill(px + 28, py + 30, 6, 6, X8_WIN_YELLOW);
    } else if (kind == X8_ICON_PAINT) {
        x8_fill(px + 8, py + 12, 48, 42, x8_color(240, 226, 190));
        x8_border(px + 8, py + 12, 48, 42, X8_WIN_BLACK);
        x8_fill(px + 14, py + 18, 8, 8, X8_WIN_RED);
        x8_fill(px + 25, py + 18, 8, 8, X8_WIN_YELLOW);
        x8_fill(px + 36, py + 18, 8, 8, X8_WIN_GREEN);
        x8_fill(px + 14, py + 29, 8, 8, X8_WIN_BLUE);
        x8_fill(px + 25, py + 29, 8, 8, X8_WIN_CYAN);
        x8_fill(px + 36, py + 29, 8, 8, X8_WIN_RED);
        x8_line(px + 18, py + 48, px + 46, py + 22, x8_color(120, 80, 40));
        x8_fill(px + 44, py + 18, 8, 8, X8_WIN_BLACK);
    } else {
        x8_fill(px + 12, py + 4, 42, 56, x8_color(240, 240, 240));
        x8_border(px + 12, py + 4, 42, 56, X8_WIN_BLACK);
        x8_fill(px + 44, py + 4, 10, 10, X8_WIN_WHITE);
        x8_line(px + 44, py + 4, px + 53, py + 13, X8_WIN_BLACK);
        for (int i = 0; i < 5; i++)
            x8_hline(px + 18, py + 20 + i * 8, 28, x8_color(90, 90, 90));
    }

    x8_text_center(r->x + 4, r->y + r->h - 22, r->w - 8,
                   icon_labels[kind], X8_WIN_BLACK, face);
}

void x8_shell_draw(int hover, int menu_open) {
    x8_shell_init();

    volatile x8_shell_shared_t *shared = X8_SHELL_SHARED;
    if (shared->magic == X8_SHELL_SHARED_MAGIC) {
        x8_window_init(&shell_win, shared->x, shared->y,
                       shared->w, shared->h, "Program Manager",
                       "File  Options  Help", X8_WF_CLOSE | X8_WF_MENU);
    }

    int sw = gfx_width(), sh = gfx_height();
    x8_desktop_fill();

    x8_fill(0, 0, sw, 24, X8_WIN_WHITE);
    x8_hline(0, 23, sw, X8_WIN_BLACK);
    x8_text(10, 5, "X8  File  Options  Help", X8_WIN_BLACK, X8_WIN_WHITE);
    x8_text_center(0, 5, sw, "Woodpile Program Manager 1.01",
                   X8_WIN_BLUE, X8_WIN_WHITE);
    x8_status_bar(sh - 28, 28,
                  "Click an icon to launch  -  Esc to exit X8  -  PS/2 mouse active");

    x8_window_draw(&shell_win, 1);

    x8_rect_t c;
    x8_window_client(&shell_win, &c);
    x8_text(c.x + 12, c.y + 12,
            "Welcome to X8 - a tiny window session for Woodpile",
            X8_WIN_BLACK, X8_WIN_WHITE);
    x8_hline(c.x + 12, c.y + 30, c.w - 24, X8_WIN_SHADOW);

    x8_rect_t icons[X8_ICON_COUNT];
    x8_shell_layout_icons(&shell_win, icons);
    for (int i = 0; i < X8_ICON_COUNT; i++)
        x8_shell_draw_icon(i, &icons[i], hover == i);

    if (menu_open) {
        x8_window_t help;
        x8_window_init(&help, shell_win.x + 48, shell_win.y + 42,
                       330, 122, "X8 Help", 0, X8_WF_CLOSE);
        x8_window_draw(&help, 1);
        x8_rect_t hc;
        x8_window_client(&help, &hc);
        x8_text(hc.x + 10, hc.y + 10, "Woodpile X8 session",
                X8_WIN_BLACK, X8_WIN_WHITE);
        x8_text(hc.x + 10, hc.y + 32, "Mouse: click icons to launch",
                X8_WIN_BLACK, X8_WIN_WHITE);
        x8_text(hc.x + 10, hc.y + 52, "Keys:  arrows, Esc to leave",
                X8_WIN_BLACK, X8_WIN_WHITE);
        x8_text(hc.x + 10, hc.y + 72, "Apps: snake, pong, paint, notes",
                X8_WIN_BLACK, X8_WIN_WHITE);
    }
}
