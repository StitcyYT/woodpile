#include "../x8.h"
#include "../x8_winapi.h"
#include "../x8_cursor.h"

#define MAX_SEG 220
#define CELL    16

static x8_cursor_t cursor;
static x8_window_t win;
static x8_drag_t win_drag;
static x8_rect_t field;
static int seg_x[MAX_SEG], seg_y[MAX_SEG];
static int seg_len, dir_x, dir_y, food_x, food_y;
static int score, game_over;
static uint32_t rng_state = 0x1234567u;

static int cols(void);
static int rows(void);
static void draw_cell(int gx, int gy, int type);
static void draw_score(void);
static void draw_background(void);
static void draw_game_over(void);
static void redraw_all(void);
static void reposition(void);

static int next_rand(int n) {
    rng_state = rng_state * 1103515245u + 12345u;
    return n > 0 ? (int)((rng_state >> 16) % (unsigned int)n) : 0;
}

static void place_food(void) {
    for (int tries = 0; tries < 80; tries++) {
        int x = next_rand(cols());
        int y = next_rand(rows());
        int hit = 0;
        for (int i = 0; i < seg_len; i++) {
            if (seg_x[i] == x && seg_y[i] == y) { hit = 1; break; }
        }
        if (!hit) {
            food_x = x;
            food_y = y;
            return;
        }
    }
    food_x = cols() / 2;
    food_y = rows() / 2;
}

static int cols(void) { return field.w / CELL; }
static int rows(void) { return (field.h - 26) / CELL; }

static void setup_window(void) {
    int sw = gfx_width(), sh = gfx_height();
    int w = sw < 1100 ? sw - 40 : 1060;
    int h = sh < 760 ? sh - 60 : 720;
    if (w < 420) w = 420;
    if (h < 360) h = 360;
    x8_window_init(&win, (sw - w) / 2, (sh - h) / 2, w, h,
                   "Snake", "Game  Help", X8_WF_CLOSE | X8_WF_MENU);
    reposition();
}

static void reposition(void) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    field = c;
    field.y += 26;
    field.h -= 26;
}

/* Score lives in its own 140x16 slot so a point only repaints that text. */
static void draw_score(void) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    x8_fill(c.x + 10, c.y + 5, 140, 16, X8_WIN_WHITE);
    char text[48];
    str_copy(text, "Score: ");
    num_to_str((unsigned long)score, text + 7);
    x8_text(c.x + 10, c.y + 5, text, X8_WIN_BLACK, X8_WIN_WHITE);
}

/* Static scenery only: window chrome, header strip, field and grid. */
static void draw_background(void) {
    x8_window_draw(&win, 1);
    x8_rect_t c;
    x8_window_client(&win, &c);
    x8_fill(c.x, c.y, c.w, 26, X8_WIN_WHITE);
    x8_text(c.x + c.w - 248, c.y + 5, "Arrows / WASD  -  R restart  -  Esc exit",
            x8_color(90, 90, 90), X8_WIN_WHITE);
    draw_score();

    unsigned int dark = x8_color(8, 58, 24);
    unsigned int grid = x8_color(12, 82, 32);
    x8_fill(field.x, field.y, cols() * CELL, rows() * CELL, dark);
    for (int x = 0; x <= cols(); x++)
        x8_vline(field.x + x * CELL, field.y, rows() * CELL, grid);
    for (int y = 0; y <= rows(); y++)
        x8_hline(field.x, field.y + y * CELL, cols() * CELL, grid);
}

/* type: 0 = empty, 1 = food, 2 = body, 3 = head.
 * Only the cell interior is touched; the 1px grid lines at the boundaries
 * are never overwritten. */
static void draw_cell(int gx, int gy, int type) {
    int ix = field.x + gx * CELL + 1;
    int iy = field.y + gy * CELL + 1;
    unsigned int dark = x8_color(8, 58, 24);
    x8_fill(ix, iy, CELL - 2, CELL - 2, dark);

    if (type == 1) {
        x8_fill(ix + 2, iy + 2, CELL - 6, CELL - 6, X8_WIN_RED);
        x8_fill(ix + 4, iy + 4, 3, 3, X8_WIN_YELLOW);
    } else if (type == 2 || type == 3) {
        unsigned int body = type == 3 ? x8_color(80, 230, 80)
                                      : x8_color(26, 170, 44);
        x8_fill(ix, iy, CELL - 2, CELL - 2, body);
        x8_border(ix, iy, CELL - 2, CELL - 2, x8_color(8, 100, 24));
        if (type == 3) {
            x8_fill(ix + 9, iy + 3, 2, 2, X8_WIN_BLACK);
            x8_fill(ix + 3, iy + 9, 2, 2, X8_WIN_BLACK);
        }
    }
}

static void draw_game_over(void) {
    int bw = 320, bh = 96;
    int bx = field.x + (cols() * CELL - bw) / 2;
    int by = field.y + (rows() * CELL - bh) / 2;
    if (bx < field.x) bx = field.x;
    if (by < field.y) by = field.y;
    x8_fill(bx, by, bw, bh, X8_WIN_BLACK);
    x8_fill(bx + 2, by + 2, bw - 4, bh - 4, X8_WIN_FACE);
    x8_border(bx + 2, by + 2, bw - 4, bh - 4, X8_WIN_WHITE);
    x8_text_center(bx, by + 22, bw, "Game Over", X8_WIN_RED, X8_WIN_FACE);
    x8_text_center(bx, by + 48, bw, "Press R to play again",
                   X8_WIN_BLACK, X8_WIN_FACE);
    x8_text_center(bx, by + 68, bw, "Esc to exit", X8_WIN_BLACK, X8_WIN_FACE);
}

static void redraw_all(void) {
    draw_background();
    draw_cell(food_x, food_y, 1);
    for (int i = seg_len - 1; i >= 0; i--)
        draw_cell(seg_x[i], seg_y[i], i == 0 ? 3 : 2);
    if (game_over) draw_game_over();
}

static void reset_game(void) {
    seg_len = 5;
    dir_x = 1;
    dir_y = 0;
    score = 0;
    game_over = 0;
    int cx = cols() / 2, cy = rows() / 2;
    for (int i = 0; i < seg_len; i++) {
        seg_x[i] = cx - i;
        seg_y[i] = cy;
    }
    place_food();
    redraw_all();
}

static void update(void) {
    int nx = seg_x[0] + dir_x;
    int ny = seg_y[0] + dir_y;

    if (nx < 0 || ny < 0 || nx >= cols() || ny >= rows()) {
        game_over = 1;
        return;
    }
    for (int i = 0; i < seg_len - 1; i++) {
        if (seg_x[i] == nx && seg_y[i] == ny) {
            game_over = 1;
            return;
        }
    }

    int growing = (nx == food_x && ny == food_y);
    int limit = seg_len;
    if (growing && seg_len < MAX_SEG) {
        limit = seg_len + 1;
        seg_len++;
        score++;
    }
    for (int i = limit - 1; i > 0; i--) {
        seg_x[i] = seg_x[i - 1];
        seg_y[i] = seg_y[i - 1];
    }
    seg_x[0] = nx;
    seg_y[0] = ny;

    if (growing) place_food();
}

static void set_direction(int nx, int ny) {
    if (dir_x != 0 && nx == -dir_x && ny == 0) return;
    if (dir_y != 0 && ny == -dir_y && nx == 0) return;
    dir_x = nx;
    dir_y = ny;
}

int xec_entry(const xec_args_t *args) {
    (void)args;
    gfx_enter();
    setup_window();
    x8_cursor_load(&cursor, "sys32/lib/cursors/Normal.cur");
    x8_drag_init(&win_drag);
    x8_cursor_hide();
    reset_game();

    uint64_t last_tick = time_ticks();
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
            x8_desktop_background();
            reposition();
            redraw_all();
        }
        if ((buttons & 1) && !(last_buttons & 1) && hit == X8_HIT_CLOSE)
            break;
        last_buttons = buttons;

        int key;
        while ((key = x8_key()) >= 0) {
            if (key == X8_KEY_ESC) goto done;
            if (key == 'r' || key == 'R') {
                x8_cursor_hide();
                reset_game();
                continue;
            }
            if (game_over) continue;
            switch (key) {
                case X8_KEY_UP:    case 'w': case 'W': set_direction(0, -1); break;
                case X8_KEY_DOWN:  case 's': case 'S': set_direction(0, 1); break;
                case X8_KEY_LEFT:  case 'a': case 'A': set_direction(-1, 0); break;
                case X8_KEY_RIGHT: case 'd': case 'D': set_direction(1, 0); break;
                default: break;
            }
        }

        uint64_t now = time_ticks();
        if (!game_over && now - last_tick >= 9) {
            /* Remember what the tick is about to change. */
            int prev_tail_x = seg_x[seg_len - 1];
            int prev_tail_y = seg_y[seg_len - 1];
            int prev_head_x = seg_x[0];
            int prev_head_y = seg_y[0];
            int prev_food_x = food_x;
            int prev_food_y = food_y;
            int prev_score = score;

            update();

            if (game_over) {
                x8_cursor_hide();
                draw_game_over();
            } else {
                int growing = (score != prev_score);
                x8_cursor_hide();
                if (!growing) draw_cell(prev_tail_x, prev_tail_y, 0);
                if (food_x != prev_food_x || food_y != prev_food_y)
                    draw_cell(prev_food_x, prev_food_y, 0);
                draw_cell(seg_x[0], seg_y[0], 3);
                draw_cell(prev_head_x, prev_head_y, 2);
                draw_cell(food_x, food_y, 1);
                if (score != prev_score) draw_score();
            }
            last_tick = now;
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
