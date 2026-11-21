#include "../x8.h"
#include "../x8_winapi.h"
#include "../x8_cursor.h"

#define MAX_ROWS 32
#define MAX_COLS 96
#define CAPTURE_SIZE 16384

static x8_cursor_t cursor;
static x8_window_t win;
static x8_drag_t win_drag;
static char capture[CAPTURE_SIZE + 1];

static char lines[MAX_ROWS][MAX_COLS + 1];
static unsigned int line_color[MAX_ROWS][MAX_COLS];
static unsigned char row_dirty[MAX_ROWS];
static int line_count = 1;
static int line_open = 1;
static int input_start;
static int rows_visible, cols_visible;
static int caret_on = 1;
static unsigned int cur_color;
static int rainbow;
static int rainbow_pos;
static int frame_dirty;
static int status_dirty;
static char status_text[96] = "qnt shell  -  Esc exit  -  drag title bar to move";

static void mark_row(int row) {
    if (row >= 0 && row < rows_visible) row_dirty[row] = 1;
}

static void mark_all_rows(void) {
    for (int row = 0; row < rows_visible; row++) row_dirty[row] = 1;
}

static void recompute(void) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    int rows = (c.h - 32) / 16;
    if (rows < 4) rows = 4;
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    int cols = (c.w - 16) / 8;
    if (cols < 20) cols = 20;
    if (cols > MAX_COLS) cols = MAX_COLS;

    if (rows != rows_visible) {
        rows_visible = rows;
        mark_all_rows();
        frame_dirty = 1;
    }
    if (cols != cols_visible) {
        cols_visible = cols;
        mark_all_rows();
    }
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
    int old_row = line_count - 1;

    if (line_count >= MAX_ROWS) {
        for (int row = 0; row < MAX_ROWS - 1; row++) {
            mem_copy(lines[row], lines[row + 1], MAX_COLS + 1);
            mem_copy(line_color[row], line_color[row + 1],
                     sizeof(line_color[row]));
        }
        line_count = MAX_ROWS;
        mark_all_rows();
    } else {
        line_count++;
    }
    /* The caret was on old_row; repaint it so the box never stamps itself
     * into the line we just finished. */
    mark_row(old_row);
    lines[line_count - 1][0] = '\0';
    line_open = 1;
    mark_row(line_count - 1);
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
    mark_row(row);
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

static void clear_screen(void) {
    /* Empty every row, not just the current one.  draw_row() still walks all
     * visible rows after a clear, so stale buffers would repaint the old text
     * immediately behind the fresh prompt. */
    for (int row = 0; row < MAX_ROWS; row++) {
        lines[row][0] = '\0';
        line_color[row][0] = X8_WIN_WHITE;
    }
    line_count = 1;
    line_open = 1;
    input_start = 0;
    rainbow = 0;
    rainbow_pos = 0;
    cur_color = X8_WIN_WHITE;
    mark_all_rows();
}

static int is_markup(const char *s, const char **start, int *len) {
    if (s[0] != '\\' || s[1] != 'c' || s[2] != ':') return 0;
    const char *p = s + 3;
    while (*p && *p != '|') p++;
    if (*p != '|') return 0;
    *start = s + 3;
    *len = (int)(p - (s + 3));
    return 1;
}

/* qnt and every command it launches write through the kernel TTY ABI.  With
 * capture enabled those bytes land here exactly as qnt emitted them, color
 * markup included; this front end only renders them. */
static void render_capture(unsigned int len) {
    if (len > CAPTURE_SIZE) len = CAPTURE_SIZE;
    capture[len] = '\0';

    const char *s = capture;
    while (*s) {
        const char *name;
        int nlen;
        if (is_markup(s, &name, &nlen)) {
            if (token_eq(name, nlen, "CLEAR")) {
                clear_screen();
            } else {
                set_markup_color(name, nlen);
            }
            s += nlen + 4;
            continue;
        }
        append_char((unsigned char)*s++);
    }
}

static int run_qnt(const char *mode, const char *text) {
    const char *argv[16];
    xec_args_t args;
    unsigned int len;
    int rc;

    for (int i = 0; i < 16; i++) argv[i] = 0;
    argv[0] = "qnt";
    argv[1] = mode;
    argv[2] = text;
    args.argc = text ? 3 : 2;
    args.reserved = 0;
    for (int i = 0; i < 16; i++) args.argv[i] = argv[i];
    args.cwd = "";

    tty_capture_set(capture, CAPTURE_SIZE);
    rc = xec_run("sys32/bin/qnt.xec", &args);
    len = tty_capture_len();
    tty_capture_set(0, 0);

    render_capture(len);
    x8_flush_keys();
    return rc;
}

static void draw_row(int row) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    int y = c.y + 4 + row * 16;
    if (row >= rows_visible || y + 16 > c.y + c.h - 22) return;

    x8_fill(c.x + 4, y, c.w - 8, 16, X8_WIN_BLACK);
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

    if (row == line_count - 1 && line_open && caret_on && len < cols_visible) {
        /* Solid block sized to the glyph area, not a full-cell hollow box. */
        int bx = c.x + 8 + len * 8;
        x8_fill(bx + 1, y + 2, 6, 12, X8_WIN_WHITE);
    }
}

static void draw_status(void) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    x8_fill(c.x, c.y + c.h - 22, c.w, 20, X8_WIN_WHITE);
    x8_hline(c.x, c.y + c.h - 23, c.w, X8_WIN_SHADOW);
    x8_text(c.x + 8, c.y + c.h - 19, status_text,
            X8_WIN_BLACK, X8_WIN_WHITE);
}

static void repaint(void) {
    if (frame_dirty) {
        x8_window_draw(&win, 1);
        x8_rect_t c;
        x8_window_client(&win, &c);
        x8_fill(c.x + 4, c.y + 4, c.w - 8, rows_visible * 16, X8_WIN_BLACK);
        frame_dirty = 0;
        mark_all_rows();
        status_dirty = 1;
    }

    for (int row = 0; row < rows_visible; row++) {
        if (row_dirty[row]) {
            draw_row(row);
            row_dirty[row] = 0;
        }
    }

    if (status_dirty) {
        draw_status();
        status_dirty = 0;
    }
}

int xec_entry(const xec_args_t *args) {
    (void)args;
    gfx_enter();

    cur_color = X8_WIN_WHITE;
    setup_window();
    x8_cursor_load(&cursor, "sys32/lib/cursors/Normal.cur");
    x8_drag_init(&win_drag);

    frame_dirty = 1;
    status_dirty = 1;
    mark_all_rows();

    /* Only qnt prints the initial prompt. */
    run_qnt("--prompt", 0);
    input_start = (int)str_len(lines[line_count - 1]);
    repaint();

    uint64_t last_blink = time_ticks();
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
            frame_dirty = 1;
            status_dirty = 1;
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
                if (run_qnt("--once", cmd) == 1) goto done;
                input_start = (int)str_len(lines[line_count - 1]);
                continue;
            }

            if (key == X8_KEY_BACK) {
                int row = line_count - 1;
                int len = (int)str_len(lines[row]);
                if (len > input_start) {
                    lines[row][len - 1] = '\0';
                    mark_row(row);
                }
                continue;
            }

            if (key >= 32 && key < 127) {
                append_char(key);
            }
        }

        uint64_t now = time_ticks();
        if (now - last_blink >= 450) {
            last_blink = now;
            caret_on = !caret_on;
            mark_row(line_count - 1);
        }

        repaint();
        x8_cursor_animate(&cursor, now);
        x8_cursor_draw(&cursor, mx, my);
        x8_sleep(4);
    }

done:
    x8_cursor_hide();
    gfx_leave();
    return 0;
}
