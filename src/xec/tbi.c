#include "xecabi.h"

/* Top Bar Information: row 0, black text on white. */
int xec_entry(const xec_args_t *args) {
    char num[24];

    /* Remember where the shell left the cursor; the bar must not move it. */
    unsigned int saved = tty_getxy();
    int saved_col = (int)(saved & 0xFFFF);
    int saved_row = (int)((saved >> 16) & 0xFFFF);

    tty_setcolor(C_BLACK, C_WHITE);
    tty_clearline(0);
    tty_setxy(0, 0);

    tty_puts("\\c:BLACK| WOODPILE ");
    tty_puts("\\c:BLACK| CPU ");
    num_to_str(time_cpu_usage(), num);
    tty_puts(num);
    tty_puts("% ");

    tty_puts("\\c:BLACK| UPTIME ");
    unsigned long secs = time_uptime_ms() / 1000;
    num_to_str(secs / 60, num);
    tty_puts(num);
    tty_puts(":");
    num_to_str(secs % 60, num);
    if (secs % 60 < 10) tty_puts("0");
    tty_puts(num);

    tty_puts("\\c:BLACK| DIR ");
    tty_puts(args && args->cwd ? args->cwd : "/");

    /* Restore the shell's normal colours and cursor position. */
    tty_setcolor(C_GRAY, C_BLACK);
    tty_setxy(saved_col, saved_row);
    tty_flush();
    return 0;
}
