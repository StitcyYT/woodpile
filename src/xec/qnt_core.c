#include "qnt_core.h"

static char cwd[64] = "sys32";

static void emit(const qnt_io_t *io, const char *s) {
    if (io && io->puts) io->puts(s, io->ctx);
}

static void emitc(const qnt_io_t *io, int c) {
    if (io && io->putc) io->putc(c, io->ctx);
}

const char *qnt_core_cwd(void) {
    return cwd;
}

void qnt_core_init(void) {
    str_copy(cwd, "sys32");
}

void qnt_core_set_cwd(const char *next) {
    if (!next) return;
    unsigned int n = 0;
    for (; next[n] && n < sizeof(cwd) - 1; n++) cwd[n] = next[n];
    cwd[n] = '\0';
}

void qnt_core_welcome(const qnt_io_t *io) {
    emit(io, "\\c:WHITE|Welcome to \\c:RAINBOW|'woodpile' shell!\\c:GRAY|\n");
}

void qnt_core_prompt(const qnt_io_t *io) {
    emit(io, "\\c:GREEN|root ");
    emit(io, "\\c:CYAN|{");
    emit(io, cwd[0] ? cwd : "/");
    emit(io, "}\\c:WHITE|# ");
    if (io && io->flush) io->flush(io->ctx);
}

static void do_cd(const char *target, const qnt_io_t *io) {
    if (!target || target[0] == '\0') {
        emit(io, cwd[0] ? cwd : "/");
        emitc(io, '\n');
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
            if (n > 0 && next[n - 1] != '/' && n < sizeof(next) - 1)
                next[n++] = '/';
        }
        i = 0;
        while (target[i] && n < sizeof(next) - 1) next[n++] = target[i++];
        next[n] = '\0';
    }

    if (!fs_is_dir(next)) {
        emit(io, "\\c:RED|cd: no such directory: ");
        emit(io, target);
        emitc(io, '\n');
        return;
    }

    str_copy(cwd, next);
}

static void print_help(const qnt_io_t *io) {
    emit(io, "\\c:WHITE|commands:\n");
    emit(io, "\\c:GREEN|  lid [dir]     list directory\n");
    emit(io, "\\c:GREEN|  rmf <file>    remove file\n");
    emit(io, "\\c:GREEN|  inf           system information\n");
    emit(io, "\\c:GREEN|  less <file>   view a file\n");
    emit(io, "\\c:GREEN|  rplay <file>  play RPLAY3 audio\n");
    emit(io, "\\c:GREEN|  x8            start the X8 window session\n");
    emit(io, "\\c:GREEN|  snake         X8 snake game\n");
    emit(io, "\\c:GREEN|  pong          X8 ping pong game\n");
    emit(io, "\\c:GREEN|  paint         X8 paint program\n");
    emit(io, "\\c:GREEN|  notepad       X8 note pad\n");
    emit(io, "\\c:GREEN|  cd [dir]      change directory\n");
    emit(io, "\\c:GREEN|  clear         clear screen\n");
    emit(io, "\\c:GREEN|  help          this list\n");
    emit(io, "\\c:GREEN|  exit          stop the shell\n");
    emit(io, "\\c:GRAY|");
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

int qnt_core_execute(const char *line, const qnt_io_t *io) {
    const char *argv[16];
    unsigned int argc = 0;
    char copy[256];
    unsigned int n = 0;

    for (; line[n] && n < sizeof(copy) - 1; n++) copy[n] = line[n];
    copy[n] = '\0';

    char *p = copy;
    while (*p && argc < 16) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }

    if (argc == 0) return 0;

    if (str_eq(argv[0], "cd")) {
        do_cd(argc > 1 ? argv[1] : 0, io);
        return 0;
    }
    if (str_eq(argv[0], "clear")) {
        if (io && io->clear) io->clear(io->ctx);
        return 0;
    }
    if (str_eq(argv[0], "help")) {
        print_help(io);
        return 0;
    }
    if (str_eq(argv[0], "exit")) {
        emit(io, "\\c:GRAY|\n");
        return 1;
    }

    char path[128];
    command_path(argv[0], path);

    xec_args_t call;
    call.argc = argc;
    call.reserved = 0;
    for (unsigned int i = 0; i < 16; i++)
        call.argv[i] = i < argc ? argv[i] : 0;
    call.cwd = cwd;

    int rc = io && io->run ? io->run(path, &call, io->ctx) : -1;
    if (rc == -1) {
        emit(io, "\\c:RED|qnt: command not found: ");
        emit(io, argv[0]);
        emitc(io, '\n');
    }
    return 0;
}
