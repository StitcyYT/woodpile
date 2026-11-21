#include "xecabi.h"

static char cwd[64] = "sys32";
static char line[256];

static void print_prompt(void) {
    tty_setcolor(C_GRAY, C_BLACK);
    tty_puts("\\c:GREEN|root ");
    tty_puts("\\c:CYAN|{");
    tty_puts(cwd[0] ? cwd : "/");
    tty_puts("}\\c:WHITE|# ");
    tty_flush();
}

static void do_cd(const char *target) {
    if (!target || target[0] == '\0') {
        tty_puts(cwd[0] ? cwd : "/");
        tty_putc('\n');
        return;
    }

    char next[64];
    unsigned int n = 0;

    if (str_eq(target, "/")) {
        next[0] = '\0';
    } else if (str_eq(target, "..")) {
        n = str_len(cwd);
        while (n > 0 && cwd[n - 1] != '/') n--;
        if (n > 0) n--;
        unsigned int i = 0;
        for (; i < n; i++) next[i] = cwd[i];
        next[n] = '\0';
    } else if (target[0] == '/') {
        unsigned int i = 0;
        while (target[i] && n < sizeof(next) - 1) next[n++] = target[i++];
        next[n] = '\0';
    } else {
        unsigned int i = 0;
        if (cwd[0]) {
            i = 0;
            while (cwd[i] && n < sizeof(next) - 1) next[n++] = cwd[i++];
            if (n > 0 && next[n - 1] != '/' && n < sizeof(next) - 1) next[n++] = '/';
        }
        i = 0;
        while (target[i] && n < sizeof(next) - 1) next[n++] = target[i++];
        next[n] = '\0';
    }

    if (!fs_is_dir(next)) {
        tty_puts("\\c:RED|cd: no such directory: ");
        tty_puts(target);
        tty_putc('\n');
        return;
    }

    str_copy(cwd, next);
}

static void print_help(void) {
    tty_puts("\\c:WHITE|commands:\n");
    tty_puts("\\c:GREEN|  lid [dir]     list directory\n");
    tty_puts("\\c:GREEN|  rmf <file>    remove file\n");
    tty_puts("\\c:GREEN|  inf           system information\n");
    tty_puts("\\c:GREEN|  less <file>   view a file\n");
    tty_puts("\\c:GREEN|  rplay <file>  play RPLAY3 audio\n");
    tty_puts("\\c:GREEN|  x8            start the X8 window session\n");
    tty_puts("\\c:GREEN|  snake         X8 snake game\n");
    tty_puts("\\c:GREEN|  pong          X8 ping pong game\n");
    tty_puts("\\c:GREEN|  paint         X8 paint program\n");
    tty_puts("\\c:GREEN|  notepad       X8 note pad\n");
    tty_puts("\\c:GREEN|  cd [dir]      change directory\n");
    tty_puts("\\c:GREEN|  clear         clear screen\n");
    tty_puts("\\c:GREEN|  help          this list\n");
    tty_puts("\\c:GREEN|  exit          stop the shell\n");
    tty_puts("\\c:GRAY|");
}

static void command_path(const char *name, char *out) {
    if (str_eq(name, "x8")) {
        str_copy(out, "sys32/bin/x8.xec");
        return;
    }
    if (str_eq(name, "snake") || str_eq(name, "games/snake")) {
        str_copy(out, "games/snake.xec");
        return;
    }
    if (str_eq(name, "pong") || str_eq(name, "pingpong") ||
        str_eq(name, "games/pong")) {
        str_copy(out, "games/pong.xec");
        return;
    }
    if (str_eq(name, "paint") || str_eq(name, "progs/paint")) {
        str_copy(out, "progs/paint.xec");
        return;
    }
    if (str_eq(name, "notepad") || str_eq(name, "note") ||
        str_eq(name, "progs/notepad")) {
        str_copy(out, "progs/notepad.xec");
        return;
    }

    unsigned int n = 0;
    if (str_str(name, "/")) {
        while (name[n] && n < 118) { out[n] = name[n]; n++; }
    } else {
        const char *prefix = "sys32/bin/";
        for (unsigned int i = 0; prefix[i]; i++) out[n++] = prefix[i];
        for (unsigned int i = 0; name[i] && n < 118; i++) out[n++] = name[i];
    }
    const char *suffix = ".xec";
    for (unsigned int i = 0; suffix[i] && n < 126; i++) out[n++] = suffix[i];
    out[n] = '\0';
}

int xec_entry(const xec_args_t *args) {
    (void)args;

    tty_clear();
    tty_setcolor(C_GRAY, C_BLACK);
    tty_puts("\\c:WHITE|Welcome to \\c:RAINBOW|'woodpile' shell!\\c:GRAY|\n");

    for (;;) {
        xec_args_t bar;
        bar.argc = 0;
        bar.reserved = 0;
        for (int i = 0; i < 16; i++) bar.argv[i] = 0;
        bar.cwd = cwd;
        (void)xec_run("sys32/bin/tbi.xec", &bar);

        print_prompt();

        kbd_getline(line, sizeof(line));

        const char *argv[16];
        unsigned int argc = 0;
        char *p = line;
        while (*p && argc < 16) {
            while (*p == ' ' || *p == '\t') p++;
            if (!*p) break;
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }

        if (argc == 0) continue;

        if (str_eq(argv[0], "cd")) {
            do_cd(argc > 1 ? argv[1] : 0);
            continue;
        }
        if (str_eq(argv[0], "clear")) {
            tty_clear();
            continue;
        }
        if (str_eq(argv[0], "help")) {
            print_help();
            continue;
        }
        if (str_eq(argv[0], "exit")) {
            tty_puts("\\c:GRAY|\n");
            return 0;
        }

        char path[128];
        command_path(argv[0], path);

        xec_args_t call;
        call.argc = argc;
        call.reserved = 0;
        for (unsigned int i = 0; i < 16; i++) call.argv[i] = i < argc ? argv[i] : 0;
        call.cwd = cwd;

        int rc = xec_run(path, &call);
        if (rc == -1) {
            tty_puts("\\c:RED|qnt: command not found: ");
            tty_puts(argv[0]);
            tty_putc('\n');
        }
    }
}
