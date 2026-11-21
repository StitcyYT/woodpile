#include "x8.h"
#include "x8_winapi.h"
#include "x8_cursor.h"
#include "x8_shell.h"

static x8_cursor_t normal_cur;
static x8_cursor_t hand_cur;
static x8_cursor_t move_cur;
static x8_cursor_t help_cur;
static x8_cursor_t busy_cur;
static x8_cursor_t *active_cur;
static x8_drag_t win_drag;

static const char *icon_paths[X8_ICON_COUNT] = {
    "games/snake.xec",
    "games/pong.xec",
    "progs/paint.xec",
    "progs/notepad.xec",
    "progs/term.xec"
};

static void use_cursor(x8_cursor_t *cur) {
    if (!cur || active_cur == cur) return;
    active_cur = cur;
    x8_cursor_reset(cur);
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

    x8_shell_init();
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
    use_cursor(&normal_cur);

    x8_window_t win;
    x8_rect_t icons[X8_ICON_COUNT];
    x8_shell_layout(&win, icons);
    x8_shell_publish(&win, -1, 0);
    x8_drag_init(&win_drag);

    int hover = -1;
    int shown_hover = -1;
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
            x8_shell_layout_icons(&win, icons);
            x8_shell_publish(&win, hover, menu_open);
            full_redraw = 1;
        } else if (drag_state == 2) {
            x8_cursor_hide();
        }

        int new_hover = -1;
        for (int i = 0; i < X8_ICON_COUNT; i++) {
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
                x8_shell_layout_icons(&win, icons);
                last_buttons = buttons;
                hover = -1;
                full_redraw = 1;
                continue;
            }
        }

        if (menu_open) {
            if (hover == X8_ICON_NOTE) use_cursor(&hand_cur);
            else use_cursor(&help_cur);
        } else if (win_hit == X8_HIT_FRAME || win_hit == X8_HIT_TITLE) {
            use_cursor(&move_cur);
        } else if (hover == X8_ICON_NOTE) {
            use_cursor(&hand_cur);
        } else {
            use_cursor(&normal_cur);
        }

        x8_cursor_animate(active_cur, time_ticks());

        if (full_redraw) {
            x8_cursor_hide();
            x8_shell_publish(&win, hover, menu_open);
            x8_shell_draw(hover, menu_open);
            shown_hover = hover;
            full_redraw = 0;
        } else if (hover != shown_hover) {
            x8_cursor_hide();
            if (shown_hover >= 0)
                x8_shell_draw_icon(shown_hover, &icons[shown_hover], 0);
            if (hover >= 0)
                x8_shell_draw_icon(hover, &icons[hover], 1);
            shown_hover = hover;
        }
        x8_cursor_draw(active_cur, mx, my);
        x8_sleep(5);
    }

    x8_cursor_hide();
    gfx_leave();
    return 0;
}
