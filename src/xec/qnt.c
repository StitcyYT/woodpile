#include "xecabi.h"
#include "qnt_core.h"

#define CWD_FILE "sys32/qnt.cwd"
#define LINE_MAX 256

static char line[LINE_MAX];
static int captured;

static void out_puts(const char *s, void *ctx) {
    (void)ctx;
    tty_puts(s);
}

static void out_putc(int c, void *ctx) {
    (void)ctx;
    tty_putc(c);
}

static void out_clear(void *ctx) {
    (void)ctx;
    if (captured) {
        /* tty_clear() only changes the hidden console while X8 owns the
         * framebuffer.  The sentinel survives raw TTY capture so the GUI
         * Terminal can clear its own text buffer. */
        tty_puts("\\c:CLEAR|");
    }
    tty_clear();
}

static void out_flush(void *ctx) {
    (void)ctx;
    tty_flush();
}

static int out_run(const char *path, const xec_args_t *args, void *ctx) {
    (void)ctx;
    return xec_run(path, args);
}

static const qnt_io_t tty_io = {
    .puts  = out_puts,
    .putc  = out_putc,
    .clear = out_clear,
    .flush = out_flush,
    .run   = out_run,
    .ctx   = 0
};

static void load_cwd(void) {
    const void *data = 0;
    uint32_t size = 0;
    char saved[64];
    unsigned int n = 0;

    if (fs_read(CWD_FILE, &data, &size) != 0 || !data) return;
    const char *bytes = (const char *)data;
    for (; n < size && n < sizeof(saved) - 1 && bytes[n]; n++)
        saved[n] = bytes[n];
    saved[n] = '\0';

    if (!saved[0] || fs_is_dir(saved)) qnt_core_set_cwd(saved);
}

static void save_cwd(void) {
    const char *cwd = qnt_core_cwd();
    unsigned int n = 0;
    while (cwd[n]) n++;
    fs_write(CWD_FILE, cwd, n);
}

static int arg_eq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void join_args(const xec_args_t *args, int first,
                      char *out, unsigned int max) {
    unsigned int n = 0;
    for (int i = first; i < (int)args->argc && i < 16; i++) {
        const char *s = args->argv[i];
        if (!s) continue;
        if (n) {
            if (n + 1 >= max) break;
            out[n++] = ' ';
        }
        while (*s && n + 1 < max) out[n++] = *s++;
    }
    out[n] = '\0';
}

int xec_entry(const xec_args_t *args) {
    qnt_core_init();
    load_cwd();

    if (args && args->argc >= 2 && args->argv[1] &&
        arg_eq(args->argv[1], "--prompt")) {
        captured = 1;
        qnt_core_prompt(&tty_io);
        return 0;
    }

    if (args && args->argc >= 2 && args->argv[1] &&
        arg_eq(args->argv[1], "--once")) {
        char once[LINE_MAX];
        int exit_shell;

        captured = 1;
        join_args(args, 2, once, sizeof(once));
        exit_shell = once[0] ? qnt_core_execute(once, &tty_io) : 0;
        save_cwd();
        if (!exit_shell) qnt_core_prompt(&tty_io);
        return exit_shell ? 1 : 0;
    }

    captured = 0;
    tty_clear();
    tty_setcolor(C_GRAY, C_BLACK);
    qnt_core_welcome(&tty_io);

    for (;;) {
        xec_args_t bar;
        bar.argc = 0;
        bar.reserved = 0;
        for (int i = 0; i < 16; i++) bar.argv[i] = 0;
        bar.cwd = qnt_core_cwd();
        (void)xec_run("sys32/bin/tbi.xec", &bar);

        qnt_core_prompt(&tty_io);
        kbd_getline(line, sizeof(line));
        if (qnt_core_execute(line, &tty_io)) return 0;
        save_cwd();
    }
}
