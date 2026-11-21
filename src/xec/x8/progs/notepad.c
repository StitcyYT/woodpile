#include "../x8.h"
#include "../x8_winapi.h"
#include "../x8_cursor.h"

#define MAX_LINES 26
#define MAX_COLS  80
#define GUTTER    48

static x8_cursor_t cursor;
static x8_window_t win;
static x8_drag_t win_drag;
static char text[MAX_LINES][MAX_COLS + 1];
static char save_buf[MAX_LINES * (MAX_COLS + 1) + MAX_LINES];
static char status_text[96] = "Ctrl+S save  -  Esc exit  -  click to place caret";
static int line_count = 1;
static int cur_line, cur_col;
static int status_dirty;
static int last_blink = -1;
static int caret_visible, caret_line, caret_col;

static void load_file(void) {
    const void *data = 0;
    uint32_t size = 0;
    if (fs_read("sys32/notepad.txt", &data, &size) != 0) {
        str_copy(text[0], "Welcome to Note Pad!");
        line_count = 1;
        return;
    }

    const unsigned char *p = (const unsigned char *)data;
    line_count = 0;
    int col = 0;
    for (uint32_t i = 0; i < size && line_count < MAX_LINES; i++) {
        unsigned char c = p[i];
        if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < size && p[i + 1] == '\n') i++;
            text[line_count][col] = '\0';
            line_count++;
            col = 0;
            continue;
        }
        if (col >= MAX_COLS) continue;
        text[line_count][col++] = (char)c;
    }
    text[line_count][col] = '\0';
    if (col > 0 || line_count == 0) line_count++;
    if (line_count > MAX_LINES) line_count = MAX_LINES;
}

static void save_file(void) {
    unsigned int n = 0;
    for (int line = 0; line < line_count; line++) {
        unsigned int len = str_len(text[line]);
        if (n + len >= sizeof(save_buf)) break;
        mem_copy(save_buf + n, text[line], len);
        n += len;
        if (line + 1 < line_count && n + 1 < sizeof(save_buf))
            save_buf[n++] = '\n';
    }
    save_buf[n] = '\0';

    if (fs_write("sys32/notepad.txt", save_buf, n) == 0)
        str_copy(status_text, "Saved sys32/notepad.txt");
    else
        str_copy(status_text, "Save failed: VFS is full");
    status_dirty = 1;
}

static void setup_window(void) {
    int sw = gfx_width(), sh = gfx_height();
    int w = sw < 1040 ? sw - 40 : 1000;
    int h = sh < 740 ? sh - 60 : 700;
    if (w < 480) w = 480;
    if (h < 420) h = 420;
    x8_window_init(&win, (sw - w) / 2, (sh - h) / 2, w, h,
                   "Note Pad", "File  Edit  Help", X8_WF_CLOSE | X8_WF_MENU);
}

static void draw_status(void) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    x8_fill(c.x, c.y + c.h - 22, c.w, 20, X8_WIN_WHITE);
    x8_hline(c.x, c.y + c.h - 23, c.w, X8_WIN_SHADOW);
    x8_text(c.x + 8, c.y + c.h - 19, status_text, X8_WIN_BLACK, X8_WIN_WHITE);
}

static void paint_caret(int line, int col) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    int caret_y = c.y + 3 + line * 16;
    if (caret_y + 16 < c.y + c.h - 24)
        x8_fill(c.x + GUTTER + 6 + col * 8, caret_y + 1, 8, 14, X8_WIN_BLUE);
}

/* Repaint a single text row.  If the caret lives on this row, put it back
 * after the text so typing never leaves the caret erased. */
static void draw_line(int line) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    int text_width = c.w - GUTTER - 8;
    x8_fill(c.x + GUTTER, c.y + 3 + line * 16, text_width, 16, X8_WIN_WHITE);
    x8_text(c.x + GUTTER + 6, c.y + 3 + line * 16, text[line],
            X8_WIN_BLACK, X8_WIN_WHITE);
    if (caret_visible && caret_line == line)
        paint_caret(line, caret_col);
}

static void draw_caret(int show, int line, int col) {
    /* A caret already on screen at a different spot has to be erased first. */
    if (caret_visible && (caret_line != line || caret_col != col)) {
        int old_line = caret_line;
        caret_visible = 0;
        draw_line(old_line);
    }
    if (!show) {
        if (caret_visible) {
            caret_visible = 0;
            draw_line(line);
        }
        return;
    }
    if (!caret_visible) {
        paint_caret(line, col);
        caret_visible = 1;
    }
    caret_line = line;
    caret_col = col;
}

static void draw_scene(int blink) {
    x8_window_draw(&win, 1);
    x8_rect_t c;
    x8_window_client(&win, &c);
    /* x8_window_draw() already fills the whole client area white. */
    x8_fill(c.x, c.y, GUTTER, c.h, X8_WIN_FACE);
    x8_vline(c.x + GUTTER - 1, c.y, c.h, X8_WIN_SHADOW);

    for (int line = 0; line < MAX_LINES && line * 16 + 16 < c.h - 24; line++) {
        char num[8];
        num_to_str((unsigned long)(line + 1), num);
        x8_text(c.x + 6, c.y + 3 + line * 16, num,
                x8_color(80, 80, 80), X8_WIN_FACE);
        x8_text(c.x + GUTTER + 6, c.y + 3 + line * 16, text[line],
                X8_WIN_BLACK, X8_WIN_WHITE);
    }

    if (blink)
        paint_caret(cur_line, cur_col);

    draw_status();
}

static void insert_char(char ch) {
    if (cur_col >= MAX_COLS - 1) return;
    int had_caret = caret_visible;
    if (had_caret) draw_caret(0, cur_line, cur_col);

    char *line = text[cur_line];

    /* Clicking past the end of a short line leaves the caret beyond the
     * NUL; pad the gap so the inserted character actually terminates
     * after the new text. */
    unsigned int len = str_len(line);
    if ((unsigned int)cur_col > len) {
        for (unsigned int i = len; i < (unsigned int)cur_col; i++)
            line[i] = ' ';
        line[cur_col] = '\0';
    }

    for (int i = MAX_COLS - 1; i > cur_col; i--) line[i] = line[i - 1];
    line[cur_col++] = ch;
    line[MAX_COLS] = '\0';

    draw_line(cur_line);
    if (had_caret) draw_caret(1, cur_line, cur_col);
}

static void backspace(void) {
    if (cur_col > 0) {
        int had_caret = caret_visible;
        if (had_caret) draw_caret(0, cur_line, cur_col);
        char *line = text[cur_line];
        cur_col--;
        for (int i = cur_col; i < MAX_COLS; i++) line[i] = line[i + 1];
        line[MAX_COLS] = '\0';
        draw_line(cur_line);
        if (had_caret) draw_caret(1, cur_line, cur_col);
        return;
    }
    if (cur_line == 0) return;

    char *prev = text[cur_line - 1];
    char *cur = text[cur_line];
    unsigned int plen = str_len(prev);
    unsigned int clen = str_len(cur);
    if (plen + clen > MAX_COLS) return;

    int had_caret = caret_visible;
    if (had_caret) draw_caret(0, cur_line, cur_col);

    for (unsigned int i = 0; i < clen; i++) prev[plen + i] = cur[i];
    prev[plen + clen] = '\0';
    cur_col = (int)plen;

    for (int i = cur_line; i < line_count - 1; i++)
        mem_copy(text[i], text[i + 1], MAX_COLS + 1);
    line_count--;
    cur_line--;

    for (int line = cur_line; line < line_count; line++)
        draw_line(line);
    if (had_caret) draw_caret(1, cur_line, cur_col);
}

static void newline(void) {
    if (cur_line >= MAX_LINES - 1 || line_count >= MAX_LINES) return;
    int had_caret = caret_visible;
    if (had_caret) draw_caret(0, cur_line, cur_col);

    for (int i = line_count; i > cur_line + 1; i--)
        mem_copy(text[i], text[i - 1], MAX_COLS + 1);
    cur_line++;
    line_count++;
    text[cur_line][0] = '\0';
    cur_col = 0;

    for (int line = cur_line; line < line_count; line++)
        draw_line(line);
    if (had_caret) draw_caret(1, cur_line, cur_col);
}

int xec_entry(const xec_args_t *args) {
    (void)args;
    gfx_enter();
    setup_window();
    x8_cursor_load(&cursor, "sys32/lib/cursors/Handwriting.cur");
    x8_drag_init(&win_drag);
    load_file();

    uint64_t start = time_ticks();
    last_blink = (int)((start / 450) & 1);
    draw_scene(last_blink);
    caret_visible = last_blink;
    caret_line = cur_line;
    caret_col = cur_col;

    int last_buttons = 0;
    for (;;) {
        /* All redraws below are partial; hide the pointer before any of them. */
        x8_cursor_hide();

        int mx = 0, my = 0, buttons = 0;
        x8_mouse(&mx, &my, &buttons);
        int hit = x8_window_hit(&win, mx, my);
        int drag_state = x8_window_drag(&win_drag, &win,
                                        mx, my, buttons, hit);
        if (drag_state == 1) {
            x8_desktop_background();
            draw_scene(caret_visible);
            caret_line = cur_line;
            caret_col = cur_col;
        }
        int clicked = (buttons & 1) && !(last_buttons & 1);
        last_buttons = buttons;

        if (clicked && drag_state == 0 && hit == X8_HIT_CLOSE)
            break;

        if (clicked && drag_state == 0) {
            x8_rect_t c;
            x8_window_client(&win, &c);
            if (x8_inside(mx, my, c.x + GUTTER, c.y,
                          c.w - GUTTER, c.h - 24)) {
                int row = (my - c.y - 3) / 16;
                int col = (mx - c.x - GUTTER - 6) / 8;
                draw_caret(0, cur_line, cur_col);
                if (row >= 0 && row < line_count) cur_line = row;
                if (col >= 0 && col <= MAX_COLS - 1) cur_col = col;
                draw_caret(1, cur_line, cur_col);
            }
        }

        int key;
        while ((key = x8_key()) >= 0) {
            if (key == X8_KEY_ESC) goto done;
            if (key == X8_KEY_CTRL_S) { save_file(); continue; }
            if (key == X8_KEY_BACK) { backspace(); continue; }
            if (key == X8_KEY_ENTER) { newline(); continue; }
            if (key == X8_KEY_UP) {
                if (cur_line > 0) {
                    draw_caret(0, cur_line, cur_col);
                    cur_line--;
                    draw_caret(1, cur_line, cur_col);
                }
                continue;
            }
            if (key == X8_KEY_DOWN) {
                if (cur_line < line_count - 1) {
                    draw_caret(0, cur_line, cur_col);
                    cur_line++;
                    draw_caret(1, cur_line, cur_col);
                }
                continue;
            }
            if (key == X8_KEY_LEFT) {
                if (cur_col > 0) {
                    draw_caret(0, cur_line, cur_col);
                    cur_col--;
                    draw_caret(1, cur_line, cur_col);
                }
                continue;
            }
            if (key == X8_KEY_RIGHT) {
                if (cur_col < (int)str_len(text[cur_line]) &&
                    cur_col < MAX_COLS - 1) {
                    draw_caret(0, cur_line, cur_col);
                    cur_col++;
                    draw_caret(1, cur_line, cur_col);
                }
                continue;
            }
            if (key >= 32 && key < 127) insert_char((char)key);
        }

        if (status_dirty) {
            draw_status();
            status_dirty = 0;
        }

        uint64_t now = time_ticks();
        int blink = (int)((now / 450) & 1);
        if (blink != last_blink) {
            last_blink = blink;
            draw_caret(blink, cur_line, cur_col);
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
