#include "xecabi.h"

int xec_entry(const xec_args_t *args) {
    (void)args;
    char num[24];

    tty_puts("\\c:WHITE|Woodpile OS 64\n");
    tty_puts("\\c:GRAY|-----------------------------\n");
    tty_puts("\\c:GREEN|kernel   : 64-bit VBE console\n");
    tty_puts("\\c:GREEN|shell    : qnt.xec\n");
    tty_puts("\\c:GREEN|commands : .xec flat binaries (XEC1)\n");
    tty_puts("\\c:GREEN|fs       : sys32/bin, sys32/lib, devc\n");

    tty_puts("\\c:CYAN|uptime   : ");
    num_to_str(time_uptime_ms() / 1000, num);
    tty_puts(num);
    tty_puts("s\n");

    tty_puts("\\c:CYAN|cpu      : ");
    num_to_str(time_cpu_usage(), num);
    tty_puts(num);
    tty_puts("%\n");

    tty_puts("\\c:GRAY|-----------------------------\n");
    return 0;
}
