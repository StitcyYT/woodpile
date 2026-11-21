#include "../x8.h"
#include "../x8_winapi.h"
#include "../x8_cursor.h"
#include "../../qnt_core.h"

#define MAX_ROWS 32
#define MAX_COLS 96

static x8_cursor_t cursor;
static x8_window_t win;
static x8_drag_t win_drag;

static char lines[MAX_ROWS][MAX_COLS + 1];
static unsigned int line_color[MAX_ROWS][MAX_COLS];
static int line_count = 1;
static int line_open = 1;
static int input_start;
static int rows_visible, cols_visible;
static int caret_on;
static unsigned int cur_color;
static int rainbow;
static int rainbow_pos;
static int full_redraw;
static int clear_requested;
static char status_text[96] = "qnt shell  -  Esc exit  -  drag title bar to move";

static void recompute(void) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    rows_visible = (c.h - 32) / 16;
    if (rows_visible < 4) rows_visible = 4;
    if (rows_visible > MAX_ROWS) rows_visible = MAX_ROWS;
    cols_visible = (c.w - 16) / 8;
    if (cols_visible < 20) cols_visible = 20;
    if (cols_visible > MAX_COLS) cols_visible = MAX_COLS;
}

static void setup_window(void) {
    int sw = gfx_width(), sh = gfx_height();
    int w = sw < 1040 ? sw - 40 : 960;
    int h = sh < 720 ? sh - 60 : 640;
    if (w < 480) w = 480;
    if (h < 360) h = 360;
    x8_window_init(&win, (sw - w) / 2, (sh - h) / 2, w, h,
                   "Terminal - qnt", "File  Edit  Help",
                   X8_WF_CLOSE | X8_WF_MENU);
    recompute();
}

static void push_line(void) {
    if (line_count >= MAX_ROWS) {
        for (int row = 0; row < MAX_ROWS - 1; row++) {
            mem_copy(lines[row], lines[row + 1], MAX_COLS + 1);
            mem_copy(line_color[row], line_color[row + 1],
                     sizeof(line_color[row]));
        }
        line_count = MAX_ROWS;
    } else {
        line_count++;
    }
    lines[line_count - 1][0] = '\0';
    line_open = 1;
}

static unsigned int palette_color(int i) {
    static const unsigned char rgb[8][3] = {
        {255, 80, 80}, {255, 180, 60}, {255, 255, 80}, {80, 220, 90},
        {80, 220, 255}, {100, 140, 255}, {210, 100, 255}, {255, 120, 190}
    };
    i &= 7;
    return x8_color(rgb[i][0], rgb[i][1], rgb[i][2]);
}

static void append_char(int ch) {
    if (ch == '\r') return;
    if (ch == '\n' || ch == '\b') {
        push_line();
        return;
    }

    int row = line_count - 1;
    int len = (int)str_len(lines[row]);
    if (len >= cols_visible || len >= MAX_COLS) {
        push_line();
        row = line_count - 1;
        len = 0;
    }

    unsigned int color = cur_color;
    if (rainbow) color = palette_color(rainbow_pos++);

    lines[row][len] = (char)ch;
    lines[row][len + 1] = '\0';
    line_color[row][len] = color;
}

/* The token between ':' and '|' is not NUL-terminated, so compare it by
 * length instead of using str_eq(). */
static int token_eq(const char *s, int len, const char *literal) {
    int i = 0;
    for (; i < len && literal[i]; i++)
        if (s[i] != literal[i]) return 0;
    return i == len && literal[i] == '\0';
}

static void set_markup_color(const char *name, int len) {
    rainbow = 0;
    if (token_eq(name, len, "WHITE")) {
        cur_color = X8_WIN_WHITE;
    } else if (token_eq(name, len, "BLACK")) {
        cur_color = X8_WIN_BLACK;
    } else if (token_eq(name, len, "RED")) {
        cur_color = x8_color(255, 90, 90);
    } else if (token_eq(name, len, "GREEN")) {
        cur_color = x8_color(90, 230, 120);
    } else if (token_eq(name, len, "BLUE")) {
        cur_color = x8_color(120, 160, 255);
    } else if (token_eq(name, len, "CYAN")) {
        cur_color = x8_color(90, 230, 230);
    } else if (token_eq(name, len, "MAGENTA")) {
        cur_color = x8_color(230, 110, 230);
    } else if (token_eq(name, len, "YELLOW")) {
        cur_color = x8_color(255, 230, 100);
    } else if (token_eq(name, len, "GRAY")) {
        cur_color = x8_color(170, 170, 170);
    } else if (token_eq(name, len, "TEAL")) {
        cur_color = x8_color(80, 200, 180);
    } else if (token_eq(name, len, "RAINBOW")) {
        rainbow = 1;
        cur_color = X8_WIN_WHITE;
    }
}

static void gui_puts(const char *s, void *ctx) {
    (void)ctx;
    if (!s) return;

    while (*s) {
        if (s[0] == '\\' && s[1] == 'c' && s[2] == ':') {
            const char *end = s + 3;
            while (*end && *end != '|') end++;
            if (*end == '|') {
                set_markup_color(s + 3, (int)(end - (s + 3)));
                s = end + 1;
                continue;
            }
        }
        append_char(*s++);
    }
    full_redraw = 1;
}

static void gui_putc(int c, void *ctx) {
    (void)ctx;
    append_char(c);
    full_redraw = 1;
}

static void gui_clear(void *ctx) {
    (void)ctx;
    line_count = 1;
    line_open = 1;
    lines[0][0] = '\0';
    input_start = 0;
    clear_requested = 1;
    full_redraw = 1;
}

static void gui_flush(void *ctx) {
    (void)ctx;
}

static int gui_run(const char *path, const xec_args_t *args, void *ctx) {
    (void)ctx;
    x8_cursor_hide();
    /* TTY commands need the text console; GUI children enter it themselves. */
    gfx_leave();
    int rc = xec_run(path, args);
    gfx_enter();
    x8_flush_keys();
    recompute();
    full_redraw = 1;
    return rc;
}

static const qnt_io_t gui_io = {
    .puts  = gui_puts,
    .putc  = gui_putc,
    .clear = gui_clear,
    .flush = gui_flush,
    .run   = gui_run,
    .ctx   = 0
};

static void draw_row(int row) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    int y = c.y + 4 + row * 16;
    x8_fill(c.x + 4, y, c.w - 8, 16, X8_WIN_BLACK);
    if (y + 16 > c.y + c.h - 24) return;

    int len = (int)str_len(lines[row]);
    if (len > cols_visible) len = cols_visible;
    for (int i = 0; i < len; i++) {
        char one[2];
        one[0] = lines[row][i];
        one[1] = '\0';
        x8_text(c.x + 8 + i * 8, y, one,
                line_color[row][i] ? line_color[row][i] : X8_WIN_WHITE,
                X8_WIN_BLACK);
    }

    if (row == line_count - 1 && line_open && caret_on && len < cols_visible)
        x8_text(c.x + 8 + len * 8, y, "_", X8_WIN_WHITE, X8_WIN_BLACK);
}

static void draw_status(void) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    x8_fill(c.x, c.y + c.h - 22, c.w, 20, X8_WIN_WHITE);
    x8_hline(c.x, c.y + c.h - 23, c.w, X8_WIN_SHADOW);
    x8_text(c.x + 8, c.y + c.h - 19, status_text,
            X8_WIN_BLACK, X8_WIN_WHITE);
}

static void draw_scene(void) {
    x8_window_draw(&win, 1);
    x8_rect_t c;
    x8_window_client(&win, &c);
    x8_fill(c.x + 4, c.y + 4, c.w - 8, rows_visible * 16, X8_WIN_BLACK);
    for (int row = 0; row < rows_visible; row++) draw_row(row);
    draw_status();
}

int xec_entry(const xec_args_t *args) {
    (void)args;
    gfx_enter();

    setup_window();
    x8_cursor_load(&cursor, "sys32/lib/cursors/Normal.cur");
    x8_drag_init(&win_drag);

    qnt_core_init();
    qnt_core_welcome(&gui_io);
    qnt_core_prompt(&gui_io);
    input_start = (int)str_len(lines[line_count - 1]);
    full_redraw = 1;

    uint64_t last_blink = time_ticks();
    int blink = 1;
    int last_buttons = 0;

    for (;;) {
        x8_cursor_hide();

        int mx = 0, my = 0, buttons = 0;
        x8_mouse(&mx, &my, &buttons);
        int hit = x8_window_hit(&win, mx, my);
        int drag_state = x8_window_drag(&win_drag, &win,
                                        mx, my, buttons, hit);
        if (drag_state == 2) {
            x8_cursor_hide();
        } else if (drag_state == 1) {
            x8_desktop_background();
            recompute();
            full_redraw = 1;
        }

        int clicked = (buttons & 1) && !(last_buttons & 1);
        last_buttons = buttons;
        if (clicked && hit == X8_HIT_CLOSE) break;

        int key;
        while ((key = x8_key()) >= 0) {
            if (key == X8_KEY_ESC) goto done;

            if (key == X8_KEY_ENTER) {
                int row = line_count - 1;
                int len = (int)str_len(lines[row]);
                char cmd[MAX_COLS + 1];
                int n = 0;
                for (int i = input_start; i < len && n < MAX_COLS; i++)
                    cmd[n++] = lines[row][i];
                cmd[n] = '\0';

                append_char('\n');
                if (qnt_core_execute(cmd, &gui_io)) goto done;
                if (clear_requested) clear_requested = 0;
                qnt_core_prompt(&gui_io);
                input_start = (int)str_len(lines[line_count - 1]);
                full_redraw = 1;
                continue;
            }

            if (key == X8_KEY_BACK) {
                int row = line_count - 1;
                int len = (int)str_len(lines[row]);
                if (len > input_start) {
                    lines[row][len - 1] = '\0';
                    full_redraw = 1;
                }
                continue;
            }

            if (key >= 32 && key < 127) {
                append_char(key);
                full_redraw = 1;
            }
        }

        uint64_t now = time_ticks();
        if (now - last_blink >= 450) {
            last_blink = now;
            blink = !blink;
            caret_on = blink;
            full_redraw = 1;
        }

        if (full_redraw) {
            draw_scene();
            full_redraw = 0;
        }

        x8_cursor_animate(&cursor, now);
        x8_cursor_draw(&cursor, mx, my);
        x8_sleep(4);
    }

done:
    x8_cursor_hide();
    gfx_leave();
    return 0;
}
