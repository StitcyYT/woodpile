#include "../x8.h"
#include "../x8_winapi.h"
#include "../x8_cursor.h"

#define MAX_ROWS 32
#define MAX_COLS 96
#define PROMPT   "root ~# "

static x8_cursor_t cursor;
static x8_window_t win;
static x8_drag_t win_drag;

static char lines[MAX_ROWS][MAX_COLS + 1];
static int line_count = 1;
static char input[MAX_COLS + 1];
static int input_len;
static int rows_visible, cols_visible;
static int caret_on;
static char status_text[96] = "Enter help  -  Esc exit  -  drag title bar to move";
static int status_dirty;
static int full_redraw;

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
                   "Terminal", "File  Edit  Help", X8_WF_CLOSE | X8_WF_MENU);
    recompute();
}

static void draw_row(int row) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    int y = c.y + 4 + row * 16;
    x8_fill(c.x + 4, y, c.w - 8, 16, X8_WIN_BLACK);
    if (y + 16 > c.y + c.h - 24) return;

    if (row == line_count - 1) {
        int px = c.x + 8;
        x8_text(px, y, PROMPT, x8_color(90, 230, 120), X8_WIN_BLACK);
        px += x8_text_width(PROMPT);
        x8_text(px, y, input, X8_WIN_WHITE, X8_WIN_BLACK);
        px += x8_text_width(input);
        if (caret_on)
            x8_text(px, y, "_", X8_WIN_WHITE, X8_WIN_BLACK);
    } else {
        x8_text(c.x + 8, y, lines[row], X8_WIN_WHITE, X8_WIN_BLACK);
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

static void draw_scene(void) {
    x8_window_draw(&win, 1);
    x8_rect_t c;
    x8_window_client(&win, &c);
    x8_fill(c.x + 4, c.y + 4, c.w - 8, rows_visible * 16, X8_WIN_BLACK);
    for (int row = 0; row < rows_visible; row++) draw_row(row);
    draw_status();
}

static void push_line(const char *s) {
    if (line_count >= MAX_ROWS) {
        mem_copy(lines[0], lines[1],
                 (MAX_ROWS - 1) * (MAX_COLS + 1));
        line_count = MAX_ROWS - 1;
    }
    str_copy(lines[line_count], s);
    int limit = cols_visible;
    if (limit > MAX_COLS) limit = MAX_COLS;
    if ((int)str_len(lines[line_count]) > limit)
        lines[line_count][limit] = '\0';
    line_count++;
}

static void say(const char *s) {
    push_line(s);
    full_redraw = 1;
}

static void run_external(const char *path) {
    if (!path[0]) return;

    xec_args_t args;
    const char *argv[16];
    for (int i = 0; i < 16; i++) argv[i] = 0;
    argv[0] = "term";
    argv[1] = path;
    args.argc = 2;
    args.reserved = 0;
    for (int i = 0; i < 16; i++) args.argv[i] = argv[i];
    args.cwd = "";

    x8_cursor_hide();
    (void)xec_run(path, &args);
    x8_flush_keys();
    x8_desktop_background();
    recompute();
    full_redraw = 1;
}

static void execute(const char *line) {
    char cmd[32], arg[MAX_COLS + 1];
    int n = 0, i = 0;

    while (line[i] == ' ') i++;
    while (line[i] && line[i] != ' ' && n < (int)sizeof(cmd) - 1)
        cmd[n++] = line[i++];
    cmd[n] = '\0';
    while (line[i] == ' ') i++;
    str_copy(arg, line + i);

    if (cmd[0] == '\0') return;

    if (str_eq(cmd, "help")) {
        say("help       show this list");
        say("clear      clear the screen");
        say("echo ...   print text");
        say("ver        show version");
        say("uptime     show ticks since boot");
        say("run PATH   launch a .xec program");
        say("exit       leave the terminal");
        return;
    }
    if (str_eq(cmd, "clear")) {
        line_count = 1;
        lines[0][0] = '\0';
        full_redraw = 1;
        return;
    }
    if (str_eq(cmd, "echo")) {
        say(arg);
        return;
    }
    if (str_eq(cmd, "ver")) {
        say("Woodpile Terminal 1.0 / X8");
        return;
    }
    if (str_eq(cmd, "uptime")) {
        char text[64];
        str_copy(text, "uptime ticks: ");
        num_to_str((unsigned long)(time_ticks() / 100), text + 14);
        say(text);
        return;
    }
    if (str_eq(cmd, "run")) {
        char text[MAX_COLS + 1];
        str_copy(text, "run ");
        str_copy(text + 4, arg);
        say(text);
        run_external(arg);
        return;
    }
    if (str_eq(cmd, "exit")) {
        str_copy(status_text, "Type Esc or click the close box to exit");
        status_dirty = 1;
        return;
    }

    char text[MAX_COLS + 1];
    str_copy(text, "unknown command: ");
    str_copy(text + 17, cmd);
    say(text);
}

int xec_entry(const xec_args_t *args) {
    (void)args;
    gfx_enter();

    setup_window();
    x8_cursor_load(&cursor, "sys32/lib/cursors/Normal.cur");
    x8_drag_init(&win_drag);
    str_copy(lines[0], "Woodpile Terminal 1.0 - type help for commands");
    draw_scene();

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
                char copy[MAX_COLS + 1];
                str_copy(copy, input);
                char shown[MAX_COLS + 1];
                str_copy(shown, PROMPT);
                str_copy(shown + str_len(PROMPT), input);
                push_line(shown);
                input_len = 0;
                input[0] = '\0';
                execute(copy);
                full_redraw = 1;
                continue;
            }
            if (key == X8_KEY_BACK) {
                if (input_len > 0) {
                    input[--input_len] = '\0';
                    draw_row(line_count - 1);
                }
                continue;
            }
            if (key >= 32 && key < 127) {
                int limit = cols_visible - (int)str_len(PROMPT) - 2;
                if (limit > MAX_COLS - 1) limit = MAX_COLS - 1;
                if (input_len < limit) {
                    input[input_len++] = (char)key;
                    input[input_len] = '\0';
                    draw_row(line_count - 1);
                }
            }
        }

        uint64_t now = time_ticks();
        if (now - last_blink >= 450) {
            last_blink = now;
            blink = !blink;
            caret_on = blink;
            draw_row(line_count - 1);
        }

        if (status_dirty) {
            draw_status();
            status_dirty = 0;
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
