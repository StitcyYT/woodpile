#include "../x8.h"
#include "../x8_winapi.h"
#include "../x8_cursor.h"

#define MAX_CANVAS_W 1280
#define MAX_CANVAS_H 720
#define PALETTE_N    16

static x8_cursor_t cursor;
static x8_window_t win;
static x8_drag_t win_drag;
static x8_rect_t canvas_rect;
static int canvas_w, canvas_h, canvas_x, canvas_y;
static unsigned char canvas[MAX_CANVAS_W * MAX_CANVAS_H];
static unsigned char save_buf[8 + PALETTE_N * 3 + MAX_CANVAS_W * MAX_CANVAS_H];
static char status_text[96] = "LMB draw  RMB erase  1-0 colors  C clear  Ctrl+S save  Esc exit";
static int selected = 1;
static int status_dirty = 0;
static int last_x, last_y, last_valid;

static const unsigned char palette[PALETTE_N][3] = {
    {255,255,255}, {0,0,0}, {190,30,30}, {30,150,40},
    {0,0,180}, {0,170,190}, {220,200,60}, {190,90,30},
    {120,70,180}, {255,140,180}, {90,90,90}, {180,220,240},
    {40,90,40}, {240,240,240}, {255,255,0}, {0,0,0}
};

static void reposition(void);

static unsigned int palette_color(int i) {
    if (i < 0 || i >= PALETTE_N) i = 0;
    return x8_color(palette[i][0], palette[i][1], palette[i][2]);
}

static void setup_window(void) {
    int sw = gfx_width(), sh = gfx_height();
    int w = sw - 80, h = sh - 100;
    if (w > 1420) w = 1420;
    if (h > 840) h = 840;
    if (w < 520) w = 520;
    if (h < 420) h = 420;
    x8_window_init(&win, (sw - w) / 2, (sh - h) / 2, w, h,
                   "Paint", "File  Edit  Colors", X8_WF_CLOSE | X8_WF_MENU);
    reposition();
}

static void reposition(void) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    canvas_x = c.x + 3;
    canvas_y = c.y + 38;
    canvas_w = c.w - 6;
    canvas_h = c.h - 38 - 26;
    if (canvas_w > MAX_CANVAS_W) canvas_w = MAX_CANVAS_W;
    if (canvas_h > MAX_CANVAS_H) canvas_h = MAX_CANVAS_H;
    if (canvas_w < 16) canvas_w = 16;
    if (canvas_h < 16) canvas_h = 16;

    canvas_rect.x = canvas_x + (c.w - 6 - canvas_w) / 2;
    canvas_rect.y = canvas_y;
    canvas_rect.w = canvas_w;
    canvas_rect.h = canvas_h;
    canvas_x = canvas_rect.x;
    canvas_y = canvas_rect.y;
}

static void plot(int x, int y, int idx) {
    if (x < 0 || y < 0 || x >= canvas_w || y >= canvas_h) return;
    if (idx < 0 || idx >= PALETTE_N) idx = 0;
    canvas[y * canvas_w + x] = (unsigned char)idx;
    gfx_pixel(canvas_x + x, canvas_y + y, palette_color(idx));
}

static void draw_line(int x0, int y0, int x1, int y1, int idx) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        plot(x0, y0, idx);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

static void clear_canvas(void) {
    mem_set(canvas, 0, (unsigned int)(canvas_w * canvas_h));
    x8_fill(canvas_x, canvas_y, canvas_w, canvas_h, palette_color(0));
}

static void draw_scene(void) {
    x8_window_draw(&win, 1);
    x8_rect_t c;
    x8_window_client(&win, &c);

    x8_fill(c.x, c.y + 3, c.w, 31, X8_WIN_FACE);
    x8_text(c.x + 8, c.y + 5, "Colors:", X8_WIN_BLACK, X8_WIN_FACE);
    for (int i = 0; i < PALETTE_N; i++) {
        int px = c.x + 68 + i * 30;
        int py = c.y + 4;
        x8_fill(px - 2, py - 2, 28, 28,
                i == selected ? X8_WIN_BLACK : X8_WIN_FACE);
        x8_fill(px, py, 24, 24, palette_color(i));
        x8_border(px, py, 24, 24, X8_WIN_DARK);
    }

    x8_fill(canvas_x, canvas_y, canvas_w, canvas_h, palette_color(0));
    for (int row = 0; row < canvas_h; row++) {
        for (int col = 0; col < canvas_w; col++) {
            unsigned int color = palette_color(canvas[row * canvas_w + col]);
            gfx_pixel(canvas_x + col, canvas_y + row, color);
        }
    }
    x8_border(canvas_x - 1, canvas_y - 1, canvas_w + 2, canvas_h + 2,
              X8_WIN_BLACK);

    x8_fill(c.x, c.y + c.h - 22, c.w, 20, X8_WIN_WHITE);
    x8_text(c.x + 8, c.y + c.h - 19, status_text, X8_WIN_BLACK, X8_WIN_WHITE);
}

/* Repaint one palette swatch (and its selection frame) in place. */
static void draw_swatch(int idx, int is_selected) {
    if (idx < 0 || idx >= PALETTE_N) return;
    x8_rect_t c;
    x8_window_client(&win, &c);
    int px = c.x + 68 + idx * 30;
    int py = c.y + 4;
    x8_fill(px - 2, py - 2, 28, 28,
            is_selected ? X8_WIN_BLACK : X8_WIN_FACE);
    x8_fill(px, py, 24, 24, palette_color(idx));
    x8_border(px, py, 24, 24, X8_WIN_DARK);
}

/* Repaint only the status strip at the bottom of the client area. */
static void draw_status(void) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    x8_fill(c.x, c.y + c.h - 22, c.w, 20, X8_WIN_WHITE);
    x8_text(c.x + 8, c.y + c.h - 19, status_text, X8_WIN_BLACK, X8_WIN_WHITE);
}

/* Change the current color and refresh just the two affected swatches. */
static void select_color(int idx) {
    if (idx < 0 || idx >= PALETTE_N || idx == selected) return;
    int old = selected;
    selected = idx;
    x8_cursor_hide();
    draw_swatch(old, 0);
    draw_swatch(idx, 1);
}

static void put16(unsigned char *p, int v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
}

static void save_canvas(void) {
    unsigned int n = 0;
    save_buf[n++] = 'X'; save_buf[n++] = '8'; save_buf[n++] = 'P'; save_buf[n++] = '1';
    put16(save_buf + n, canvas_w); n += 2;
    put16(save_buf + n, canvas_h); n += 2;
    for (int i = 0; i < PALETTE_N; i++) {
        save_buf[n++] = palette[i][0];
        save_buf[n++] = palette[i][1];
        save_buf[n++] = palette[i][2];
    }
    mem_copy(save_buf + n, canvas, (unsigned int)(canvas_w * canvas_h));
    n += (unsigned int)(canvas_w * canvas_h);

    if (fs_write("sys32/paint.x8p", save_buf, n) == 0)
        str_copy(status_text, "Saved sys32/paint.x8p");
    else
        str_copy(status_text, "Save failed: VFS is full");
    status_dirty = 1;
}

int xec_entry(const xec_args_t *args) {
    (void)args;
    gfx_enter();
    setup_window();
    x8_cursor_load(&cursor, "sys32/lib/cursors/Handwriting.cur");
    x8_drag_init(&win_drag);
    draw_scene();  /* one-time full draw; all later updates are partial */

    int last_buttons = 0;
    for (;;) {
        int mx = 0, my = 0, buttons = 0;
        x8_mouse(&mx, &my, &buttons);
        int hit = x8_window_hit(&win, mx, my);
        int drag_state = x8_window_drag(&win_drag, &win,
                                        mx, my, buttons, hit);
        if (drag_state == 2) {
            x8_cursor_hide();
        } else if (drag_state == 1) {
            x8_cursor_hide();
            x8_desktop_background();
            reposition();
            draw_scene();
            last_valid = 0;
        }

        /* Only erase the pointer when this frame really changes pixels
         * underneath it.  Hiding it every iteration was the jitter source.
         * Swatch/status repaints hide it themselves right before drawing. */
        int inside = x8_inside(mx, my, canvas_x, canvas_y,
                               canvas_w, canvas_h);
        int drawing = (buttons & 3) && inside;
        if (drawing) x8_cursor_hide();

        int clicked = (buttons & 1) && !(last_buttons & 1);
        if (clicked && drag_state == 0 && hit == X8_HIT_CLOSE)
            break;

        x8_rect_t c;
        x8_window_client(&win, &c);
        if (clicked && drag_state == 0 &&
            x8_inside(mx, my, c.x + 66, c.y + 2, PALETTE_N * 30, 28)) {
            select_color((mx - (c.x + 68)) / 30);
        }
        last_buttons = buttons;

        int key;
        while ((key = x8_key()) >= 0) {
            if (key == X8_KEY_ESC) goto done;
            if (key == X8_KEY_CTRL_S) { save_canvas(); continue; }
            if (key == 'c' || key == 'C') { clear_canvas(); continue; }
            if (key >= '1' && key <= '9') { select_color(key - '1'); continue; }
            if (key == '0') { select_color(9); continue; }
        }

        if (status_dirty) {
            x8_cursor_hide();
            draw_status();
            status_dirty = 0;
        }

        int cx = mx - canvas_x;
        int cy = my - canvas_y;

        if (drawing) {
            int idx = (buttons & 1) ? selected : 0;
            if (last_valid && x8_inside(last_x, last_y, canvas_x, canvas_y,
                                        canvas_w, canvas_h))
                draw_line(last_x - canvas_x, last_y - canvas_y, cx, cy, idx);
            else
                plot(cx, cy, idx);
            last_x = mx;
            last_y = my;
            last_valid = 1;
        } else {
            last_valid = 0;
        }

        x8_cursor_draw(&cursor, mx, my);
        x8_sleep(4);
    }

done:
    x8_cursor_hide();
    gfx_leave();
    return 0;
}
