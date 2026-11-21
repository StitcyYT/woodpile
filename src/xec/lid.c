#include "xecabi.h"

int xec_entry(const xec_args_t *args) {
    const char *dir = (args && args->argc > 1) ? args->argv[1] : (args ? args->cwd : "/");
    char list[2048];

    int n = fs_list(dir, list, sizeof(list));
    if (n < 0) {
        tty_puts("\\c:RED|lid: cannot list ");
        tty_puts(dir);
        tty_puts("\n");
        return 1;
    }

    const char *p = list;
    while (*p) {
        const char *start = p;
        while (*p && *p != '\n') p++;
        unsigned int len = (unsigned int)(p - start);

        char name[64];
        unsigned int i = 0;
        for (; i < len && i < sizeof(name) - 1; i++) name[i] = start[i];
        name[i] = '\0';

        if (i > 0 && name[i - 1] == '/')
            tty_puts("\\c:CYAN|");
        else
            tty_puts("\\c:WHITE|");
        tty_puts(name);
        tty_putc('\n');

        if (*p == '\n') p++;
    }

    tty_puts("\\c:GRAY|");
    return 0;
}
