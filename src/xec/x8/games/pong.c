#include "../x8.h"
#include "../x8_winapi.h"
#include "../x8_cursor.h"

#define PAD_W 12
#define PAD_H 68
#define BALL 10
#define FIELD_COLOR x8_color(4, 22, 62)
#define SCORE_SLOT_W 24
#define SCORE_GAP 6

static x8_cursor_t cursor;
static x8_window_t win;
static x8_rect_t field;
static int ball_x, ball_y, vx, vy;
static int left_y, right_y;
static int score_l, score_r;
static int prev_ball_x, prev_ball_y;
static int prev_left_y, prev_right_y;
static int prev_score_l, prev_score_r;
static uint32_t rng_state = 0x9E3779B9u;

static int rand_n(int n) {
    rng_state = rng_state * 1103515245u + 12345u;
    return n > 0 ? (int)((rng_state >> 16) % (unsigned int)n) : 0;
}

static void setup_window(void) {
    int sw = gfx_width(), sh = gfx_height();
    int w = sw < 1100 ? sw - 40 : 1060;
    int h = sh < 760 ? sh - 60 : 720;
    if (w < 420) w = 420;
    if (h < 360) h = 360;
    x8_window_init(&win, (sw - w) / 2, (sh - h) / 2, w, h,
                   "Ping Pong", "Game  Help", X8_WF_CLOSE | X8_WF_MENU);

    x8_rect_t c;
    x8_window_client(&win, &c);
    field = c;
    field.y += 26;
    field.h -= 26;
}

static void reset_ball(int toward_left) {
    ball_x = field.w / 2 - BALL / 2;
    ball_y = field.h / 2 - BALL / 2;
    vx = toward_left ? -4 : 4;
    vy = (rand_n(5) - 2);
    if (vy == 0) vy = 1;
}

static void reset_game(void) {
    score_l = 0;
    score_r = 0;
    left_y = field.h / 2 - PAD_H / 2;
    right_y = left_y;
    reset_ball(rand_n(2));
}

/* Everything that never moves is painted exactly once at startup. */
static void draw_background(void) {
    x8_window_draw(&win, 1);

    x8_rect_t c;
    x8_window_client(&win, &c);
    x8_fill(c.x, c.y, c.w, 26, X8_WIN_WHITE);
    x8_text(c.x + 10, c.y + 5, "You: ", X8_WIN_BLACK, X8_WIN_WHITE);
    int lx = c.x + 10 + x8_text_width("You: ");
    x8_text(lx + SCORE_SLOT_W + SCORE_GAP, c.y + 5, "CPU: ",
            X8_WIN_BLACK, X8_WIN_WHITE);
    x8_text(c.x + c.w - 250, c.y + 5, "Mouse / W-S  -  Esc exit",
            x8_color(90, 90, 90), X8_WIN_WHITE);

    x8_fill(field.x, field.y, field.w, field.h, FIELD_COLOR);
    x8_border(field.x, field.y, field.w, field.h, X8_WIN_WHITE);
    for (int y = field.y + 8; y < field.y + field.h - 8; y += 18)
        x8_fill(field.x + field.w / 2 - 1, y, 2, 9, X8_WIN_WHITE);
}

/* Only the two small white-backed score slots are repainted on a score. */
static void draw_scores(void) {
    x8_rect_t c;
    x8_window_client(&win, &c);
    char text[16];

    int lx = c.x + 10 + x8_text_width("You: ");
    int rx = lx + SCORE_SLOT_W + SCORE_GAP + x8_text_width("CPU: ");

    num_to_str((unsigned long)score_l, text);
    x8_fill(lx, c.y + 4, SCORE_SLOT_W, 18, X8_WIN_WHITE);
    x8_text(lx, c.y + 5, text, X8_WIN_BLACK, X8_WIN_WHITE);

    num_to_str((unsigned long)score_r, text);
    x8_fill(rx, c.y + 4, SCORE_SLOT_W, 18, X8_WIN_WHITE);
    x8_text(rx, c.y + 5, text, X8_WIN_BLACK, X8_WIN_WHITE);
}

static void draw_ball(int x, int y, int erase) {
    int rx = field.x + x;
    int ry = field.y + y;
    int rw = BALL, rh = BALL;

    /* A ball near a goal line is partly outside the field; clip it so an
     * erase never paints over the border that draw_background() installed. */
    if (rx < field.x) { rw += rx - field.x; rx = field.x; }
    if (ry < field.y) { rh += ry - field.y; ry = field.y; }
    if (rx + rw > field.x + field.w) rw = field.x + field.w - rx;
    if (ry + rh > field.y + field.h) rh = field.y + field.h - ry;
    if (rw <= 0 || rh <= 0) return;

    x8_fill(rx, ry, rw, rh, erase ? FIELD_COLOR : X8_WIN_YELLOW);
}

static void draw_paddle(int is_left, int y, int erase) {
    int x = is_left ? field.x + 8 : field.x + field.w - 8 - PAD_W;
    x8_fill(x, field.y + y, PAD_W, PAD_H,
            erase ? FIELD_COLOR : X8_WIN_WHITE);
}

static void update(void) {
    ball_x += vx;
    ball_y += vy;

    if (ball_y <= 0) { ball_y = 0; vy = -vy; }
    if (ball_y + BALL >= field.h) {
        ball_y = field.h - BALL;
        vy = -vy;
    }

    int left_x = 8 + PAD_W;
    int right_x = field.w - 8 - PAD_W;

    if (vx < 0 && ball_x <= left_x && ball_x + BALL >= 8 &&
        ball_y + BALL >= left_y && ball_y <= left_y + PAD_H) {
        ball_x = left_x;
        vx = -vx + 1;
        vy += (ball_y + BALL / 2) < (left_y + PAD_H / 2) ? -1 : 1;
        if (vy == 0) vy = 1;
    }

    if (vx > 0 && ball_x + BALL >= right_x && ball_x <= right_x + PAD_W &&
        ball_y + BALL >= right_y && ball_y <= right_y + PAD_H) {
        ball_x = right_x - BALL;
        vx = -(vx + 1);
        vy += (ball_y + BALL / 2) < (right_y + PAD_H / 2) ? -1 : 1;
        if (vy == 0) vy = -1;
    }

    if (vx > 7) vx = 7;
    if (vx < -7) vx = -7;
    if (vy > 4) vy = 4;
    if (vy < -4) vy = -4;

    int target = ball_y + BALL / 2 - PAD_H / 2;
    if (target < 0) target = 0;
    if (target > field.h - PAD_H) target = field.h - PAD_H;
    if (right_y < target) right_y++;
    else if (right_y > target) right_y--;

    if (ball_x + BALL < 0) {
        score_r++;
        reset_ball(0);
    } else if (ball_x > field.w) {
        score_l++;
        reset_ball(1);
    }
}

int xec_entry(const xec_args_t *args) {
    (void)args;
    gfx_enter();
    setup_window();
    x8_cursor_load(&cursor, "sys32/lib/cursors/Normal.cur");
    reset_game();

    draw_background();
    draw_scores();
    prev_ball_x = ball_x;
    prev_ball_y = ball_y;
    prev_left_y = left_y;
    prev_right_y = right_y;
    prev_score_l = score_l;
    prev_score_r = score_r;

    uint64_t last_tick = time_ticks();
    int last_buttons = 0;

    for (;;) {
        int mx = 0, my = 0, buttons = 0;
        x8_mouse(&mx, &my, &buttons);
        if ((buttons & 1) && !(last_buttons & 1) &&
            x8_window_hit(&win, mx, my) == X8_HIT_CLOSE)
            break;
        last_buttons = buttons;

        int key;
        while ((key = x8_key()) >= 0) {
            if (key == X8_KEY_ESC) goto done;
            if (key == X8_KEY_UP || key == 'w' || key == 'W') left_y -= 10;
            if (key == X8_KEY_DOWN || key == 's' || key == 'S') left_y += 10;
        }
        if (x8_inside(mx, my, field.x, field.y, field.w, field.h))
            left_y = my - field.y - PAD_H / 2;
        if (left_y < 0) left_y = 0;
        if (left_y > field.h - PAD_H) left_y = field.h - PAD_H;

        uint64_t now = time_ticks();
        if (now - last_tick >= 16) {
            update();
            last_tick = now;
        }

        x8_cursor_animate(&cursor, now);
        x8_cursor_hide();

        if (score_l != prev_score_l || score_r != prev_score_r) {
            draw_scores();
            prev_score_l = score_l;
            prev_score_r = score_r;
        }

        /* Erase the previous frame's moving sprites, then paint the current
         * ones. Mouse/key changes happen outside update(), so this runs every
         * frame with fresh previous-position tracking. */
        draw_paddle(1, prev_left_y, 1);
        draw_paddle(0, prev_right_y, 1);
        draw_ball(prev_ball_x, prev_ball_y, 1);

        draw_paddle(1, left_y, 0);
        draw_paddle(0, right_y, 0);
        draw_ball(ball_x, ball_y, 0);

        prev_ball_x = ball_x;
        prev_ball_y = ball_y;
        prev_left_y = left_y;
        prev_right_y = right_y;

        x8_cursor_draw(&cursor, mx, my);
        x8_sleep(3);
    }

done:
    x8_cursor_hide();
    gfx_leave();
    return 0;
}
