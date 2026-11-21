#include "xecabi.h"

/* Build the same cwd-relative path less uses.  A leading '/' is kept so the
 * path reads like the absolute form the user would otherwise have to type. */
static void full_path(const char *name, const char *cwd, char *out) {
    unsigned int n = 0;
    if (name[0] == '/') {
        while (name[n]) { out[n] = name[n]; n++; }
    } else {
        unsigned int i = 0;
        while (cwd && cwd[i]) { out[n++] = cwd[i++]; }
        if (n > 0 && out[n - 1] != '/') out[n++] = '/';
        i = 0;
        while (name[i]) { out[n++] = name[i++]; }
    }
    out[n] = '\0';
}

int xec_entry(const xec_args_t *args) {
    const char *dir = (args && args->argc > 1) ? args->argv[1] : (args ? args->cwd : "/");
    char path[160];
    const char *list_path = dir;
    char list[2048];

    if (args && args->argc > 1 && dir[0] != '/') {
        full_path(dir, args->cwd, path);
        /* Try the cwd first, then fall back to a root-relative directory. */
        if (fs_list(path, list, sizeof(list)) >= 0)
            list_path = path;
    }

    int n = fs_list(list_path, list, sizeof(list));
    if (n < 0) {
        tty_puts("\\c:RED|lid: cannot list ");
        tty_puts(list_path);
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
