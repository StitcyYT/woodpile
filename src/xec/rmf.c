#include "xecabi.h"

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
    if (!args || args->argc < 2) {
        tty_puts("\\c:RED|usage: rmf <file> [file ...]\n");
        return 1;
    }

    for (unsigned int i = 1; i < args->argc && i < 16; i++) {
        char path[128];
        full_path(args->argv[i], args ? args->cwd : 0, path);
        if (fs_remove(path) != 0) {
            tty_puts("\\c:RED|rmf: cannot remove ");
            tty_puts(path);
            tty_putc('\n');
        }
    }
    tty_puts("\\c:GRAY|");
    return 0;
}
