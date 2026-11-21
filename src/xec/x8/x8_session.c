#include "x8.h"
#include "x8_winapi.h"
#include "x8_cursor.h"

typedef enum {
    ICON_SNAKE = 0,
    ICON_PONG,
    ICON_PAINT,
    ICON_NOTE,
    ICON_TERM,
    ICON_COUNT
} x8_app_icon_t;

static x8_cursor_t normal_cur;
static x8_cursor_t hand_cur;
static x8_cursor_t move_cur;
static x8_cursor_t help_cur;
static x8_cursor_t busy_cur;
static x8_cursor_t *active_cur;
static x8_cursor_t app_icons[ICON_COUNT];
static x8_drag_t win_drag;

static const char *icon_labels[ICON_COUNT] = {
    "Snake", "Ping Pong", "Paint", "Note Pad", "Terminal"
};

static const char *icon_paths[ICON_COUNT] = {
    "games/snake.xec",
    "games/pong.xec",
    "progs/paint.xec",
    "progs/notepad.xec",
    "progs/term.xec"
};

static const char *icon_image_paths[ICON_COUNT] = {
    "sys32/icons/snake.ico",
    "sys32/icons/pingpong.ico",
    "sys32/icons/paint.ico",
    "sys32/icons/notepad.ico",
    "sys32/icons/term.ico"
};

static void use_cursor(x8_cursor_t *cur) {
    if (!cur || active_cur == cur) return;
    active_cur = cur;
    x8_cursor_reset(cur);
}

static void layout_icons(const x8_window_t *win, x8_rect_t icons[ICON_COUNT]);

static void layout(x8_window_t *win, x8_rect_t icons[ICON_COUNT]) {
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
    layout_icons(win, icons);
}

static void layout_icons(const x8_window_t *win, x8_rect_t icons[ICON_COUNT]) {
    x8_rect_t c;
    x8_window_client(win, &c);
    int tw = 168, th = 136, gap_x = 24, gap_y = 18;
    int rows = (ICON_COUNT + 1) / 2;
    int grid_w = tw * 2 + gap_x;
    int grid_h = th * rows + gap_y * (rows - 1);
    int ox = c.x + (c.w - grid_w) / 2;
    int oy = c.y + 42 + ((c.h - 42 - grid_h) / 2);
    if (oy < c.y + 34) oy = c.y + 34;

    for (int i = 0; i < ICON_COUNT; i++) {
        icons[i].x = ox + (i % 2) * (tw + gap_x);
        icons[i].y = oy + (i / 2) * (th + gap_y);
        icons[i].w = tw;
        icons[i].h = th;
    }
}

static void draw_icon(int kind, const x8_rect_t *r, int hover) {
    unsigned int face = hover ? x8_color(220, 236, 250) : X8_WIN_WHITE;
    x8_fill(r->x, r->y, r->w, r->h, face);
    x8_border(r->x, r->y, r->w, r->h, hover ? X8_WIN_BLUE : X8_WIN_SHADOW);
    x8_hline(r->x + 1, r->y + 1, r->w - 2, X8_WIN_WHITE);
    x8_vline(r->x + 1, r->y + 1, 1, X8_WIN_WHITE);

    int px = r->x + (r->w - 64) / 2;
    int py = r->y + 12;

    /* Draw the real ICO from assets/icons when it loaded.  The hand-drawn
     * shapes below stay as a fallback if an asset is missing. */
    if (app_icons[kind].count > 0) {
        x8_image_draw(&app_icons[kind], px, py, 4);
    } else if (kind == ICON_SNAKE) {
        x8_fill(px + 4, py + 4, 56, 52, x8_color(12, 74, 22));
        x8_border(px + 4, py + 4, 56, 52, X8_WIN_BLACK);
        x8_fill(px + 14, py + 28, 12, 10, x8_color(40, 200, 50));
        x8_fill(px + 24, py + 28, 12, 10, x8_color(30, 170, 40));
        x8_fill(px + 34, py + 28, 12, 10, x8_color(30, 170, 40));
        x8_fill(px + 42, py + 20, 10, 10, x8_color(50, 220, 60));
        x8_fill(px + 47, py + 23, 2, 2, X8_WIN_BLACK);
        x8_fill(px + 16, py + 10, 8, 8, X8_WIN_RED);
    } else if (kind == ICON_PONG) {
        x8_fill(px + 4, py + 4, 56, 52, x8_color(0, 30, 90));
        x8_border(px + 4, py + 4, 56, 52, X8_WIN_WHITE);
        for (int y = py + 8; y < py + 52; y += 10)
            x8_fill(px + 31, y, 2, 5, X8_WIN_WHITE);
        x8_fill(px + 8, py + 16, 5, 22, X8_WIN_WHITE);
        x8_fill(px + 51, py + 24, 5, 22, X8_WIN_WHITE);
        x8_fill(px + 28, py + 30, 6, 6, X8_WIN_YELLOW);
    } else if (kind == ICON_PAINT) {
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

static void draw_scene(const x8_window_t *win, const x8_rect_t icons[ICON_COUNT],
                       int hover, int menu_open) {
    int sw = gfx_width(), sh = gfx_height();
    x8_checker(0, 24, sw, sh - 52, 24);

    x8_fill(0, 0, sw, 24, X8_WIN_WHITE);
    x8_hline(0, 23, sw, X8_WIN_BLACK);
    x8_text(10, 5, "X8  File  Options  Help", X8_WIN_BLACK, X8_WIN_WHITE);
    x8_text_center(0, 5, sw, "Woodpile Program Manager 1.01",
                   X8_WIN_BLUE, X8_WIN_WHITE);

    x8_status_bar(sh - 28, 28,
                  "Click an icon to launch  -  Esc to exit X8  -  PS/2 mouse active");

    x8_window_draw(win, 1);

    x8_rect_t c;
    x8_window_client(win, &c);
    x8_text(c.x + 12, c.y + 12,
            "Welcome to X8 - a tiny window session for Woodpile",
            X8_WIN_BLACK, X8_WIN_WHITE);
    x8_hline(c.x + 12, c.y + 30, c.w - 24, X8_WIN_SHADOW);

    for (int i = 0; i < ICON_COUNT; i++)
        draw_icon(i, &icons[i], hover == i);

    if (menu_open) {
        x8_window_t help;
        x8_window_init(&help, win->x + 48, win->y + 42, 330, 122,
                       "X8 Help", 0, X8_WF_CLOSE);
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

static void launch_app(const char *path, int mx, int my) {
    x8_cursor_hide();
    use_cursor(&busy_cur);
    x8_cursor_draw(active_cur, mx, my);
    x8_sleep(80);
    x8_cursor_hide();

    xec_args_t args;
    const char *argv[16];
    for (int i = 0; i < 16; i++) argv[i] = 0;
    argv[0] = "x8";
    args.argc = 1;
    args.reserved = 0;
    for (int i = 0; i < 16; i++) args.argv[i] = argv[i];
    args.cwd = "";
    (void)xec_run(path, &args);
    /* The app may have consumed an Esc (or other key) to exit; don't let
     * any remaining press/release events close the parent session. */
    x8_flush_keys();
}

int xec_entry(const xec_args_t *args) {
    (void)args;

    gfx_enter();

    if (x8_cursor_load(&normal_cur, "sys32/lib/cursors/Normal.cur") != 0)
        serial_puts("x8: Normal.cur failed\n");
    if (x8_cursor_load(&hand_cur, "sys32/lib/cursors/Handwriting.cur") != 0)
        serial_puts("x8: Handwriting.cur failed\n");
    if (x8_cursor_load(&move_cur, "sys32/lib/cursors/Move.cur") != 0)
        serial_puts("x8: Move.cur failed\n");
    if (x8_cursor_load(&help_cur, "sys32/lib/cursors/Help.cur") != 0)
        serial_puts("x8: Help.cur failed\n");
    if (x8_cursor_load(&busy_cur, "sys32/lib/cursors/Busy.ani") != 0)
        serial_puts("x8: Busy.ani failed\n");
    for (int i = 0; i < ICON_COUNT; i++) {
        if (x8_cursor_load(&app_icons[i], icon_image_paths[i]) != 0)
            serial_puts("x8: app icon failed\n");
    }
    use_cursor(&normal_cur);

    x8_window_t win;
    x8_rect_t icons[ICON_COUNT];
    layout(&win, icons);
    x8_drag_init(&win_drag);

    int hover = -1;
    int shown_hover = -1;   /* icon state currently painted on screen */
    int menu_open = 0;
    int last_buttons = 0;
    int full_redraw = 1;

    for (;;) {
        int mx = 0, my = 0, buttons = 0;
        x8_mouse(&mx, &my, &buttons);

        int win_hit = x8_window_hit(&win, mx, my);
        int drag_state = x8_window_drag(&win_drag, &win,
                                        mx, my, buttons, win_hit);
        if (drag_state == 1) {
            layout_icons(&win, icons);
            full_redraw = 1;
        } else if (drag_state == 2) {
            x8_cursor_hide();
        }

        int new_hover = -1;
        for (int i = 0; i < ICON_COUNT; i++) {
            if (x8_inside(mx, my, icons[i].x, icons[i].y,
                          icons[i].w, icons[i].h)) {
                new_hover = i;
                break;
            }
        }
        hover = new_hover;

        int clicked = (buttons & 1) && !(last_buttons & 1);
        last_buttons = buttons;

        int key = x8_key();
        if (key == X8_KEY_ESC) break;

        x8_window_t popup;
        x8_window_init(&popup, win.x + 48, win.y + 42, 330, 122,
                       "X8 Help", 0, X8_WF_CLOSE);
        int popup_hit = x8_window_hit(&popup, mx, my);

        if (clicked && drag_state == 0) {
            if (menu_open && popup_hit == X8_HIT_CLOSE) {
                menu_open = 0;
                full_redraw = 1;
            } else if (menu_open && popup_hit == X8_HIT_NONE) {
                menu_open = 0;
                full_redraw = 1;
            } else if (win_hit == X8_HIT_CLOSE) {
                break;
            } else if (x8_inside(mx, my, win.x + 3, win.y + 26,
                                 win.w - 6, 20)) {
                menu_open = !menu_open;
                full_redraw = 1;
            } else if (hover >= 0 && !menu_open) {
                launch_app(icon_paths[hover], mx, my);
                layout_icons(&win, icons);
                last_buttons = buttons;
                hover = -1;
                full_redraw = 1;
                continue;
            }
        }

        if (menu_open) {
            if (hover == ICON_NOTE) use_cursor(&hand_cur);
            else use_cursor(&help_cur);
        } else if (win_hit == X8_HIT_FRAME || win_hit == X8_HIT_TITLE) {
            use_cursor(&move_cur);
        } else if (hover == ICON_NOTE) {
            use_cursor(&hand_cur);
        } else {
            use_cursor(&normal_cur);
        }

        x8_cursor_animate(active_cur, time_ticks());

        /* Full repaints (checkerboard, bars, window) are rare.  Normal hover
         * changes only touch the two icon rectangles, so the desktop doesn't
         * flash every time the pointer crosses an icon. */
        if (full_redraw) {
            x8_cursor_hide();
            draw_scene(&win, icons, hover, menu_open);
            shown_hover = hover;
            full_redraw = 0;
        } else if (hover != shown_hover) {
            x8_cursor_hide();
            if (shown_hover >= 0)
                draw_icon(shown_hover, &icons[shown_hover], 0);
            if (hover >= 0)
                draw_icon(hover, &icons[hover], 1);
            shown_hover = hover;
        }
        x8_cursor_draw(active_cur, mx, my);
        x8_sleep(5);
    }

    x8_cursor_hide();
    gfx_leave();
    return 0;
}
