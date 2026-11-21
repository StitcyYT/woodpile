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

/* `bat` is Woodpile's cat: dump each named file byte-for-byte, no pager. */
int xec_entry(const xec_args_t *args) {
    if (!args || args->argc < 2) {
        tty_puts("\\c:RED|usage: bat <file> [file ...]\n");
        return 1;
    }

    for (unsigned int i = 1; i < args->argc && i < 16; i++) {
        char path[128];
        full_path(args->argv[i], args->cwd, path);

        const void *raw = 0;
        uint32_t size = 0;
        if (fs_read(path, &raw, &size) != 0) {
            if (args->argv[i][0] != '/' &&
                fs_read(args->argv[i], &raw, &size) == 0) {
                /* root-relative fallback */
            } else {
                tty_puts("\\c:RED|bat: cannot open ");
                tty_puts(path);
                tty_putc('\n');
                continue;
            }
        }

        const unsigned char *bytes = (const unsigned char *)raw;
        for (uint32_t n = 0; n < size; n++)
            tty_putc((int)bytes[n]);
    }

    tty_puts("\\c:GRAY|");
    return 0;
}
