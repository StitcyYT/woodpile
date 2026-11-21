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
        tty_puts("\\c:RED|usage: less <file>\n");
        return 1;
    }

    char path[128];
    full_path(args->argv[1], args->cwd, path);

    const void *raw = 0;
    uint32_t size = 0;
    if (fs_read(path, &raw, &size) != 0) {
        /* Accept a root-relative name too.  A file can live at /file.txt
         * while the shell cwd is sys32, and requiring /file.txt explicitly
         * is needlessly strict. */
        if (args->argv[1][0] != '/' &&
            fs_read(args->argv[1], &raw, &size) == 0) {
            /* loaded from root */
        } else {
            tty_puts("\\c:RED|less: cannot open ");
            tty_puts(path);
            tty_putc('\n');
            return 1;
        }
    }

    static char text[8192];
    unsigned int n = size < sizeof(text) - 1 ? size : sizeof(text) - 1;
    mem_copy(text, raw, n);
    text[n] = '\0';

    int per_page = tty_rows() - 3;
    if (per_page < 1) per_page = 1;

    const char *p = text;
    int line_no = 0;
    while (*p) {
        const char *start = p;
        while (*p && *p != '\n') p++;

        char line[256];
        unsigned int i = 0;
        for (; i < (unsigned int)(p - start) && i < sizeof(line) - 1; i++)
            line[i] = start[i];
        line[i] = '\0';

        tty_puts("\\c:WHITE|");
        tty_puts(line);
        tty_putc('\n');
        if (*p == '\n') p++;

        line_no++;
        if (line_no >= per_page && *p) {
            tty_puts("\\c:YELLOW|-- more -- (any key)");
            tty_flush();
            (void)kbd_getc();
            tty_putc('\n');
            line_no = 0;
        }
    }

    tty_puts("\\c:GRAY|");
    return 0;
}
