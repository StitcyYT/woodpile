#include "xecabi.h"
#include "qnt_core.h"

static char line[256];

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

int xec_entry(const xec_args_t *args) {
    (void)args;

    qnt_core_init();
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
    }
}
