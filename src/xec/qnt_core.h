#ifndef QNT_CORE_H
#define QNT_CORE_H

#include "xecabi.h"

/* qnt's shell engine with front-end supplied I/O.  The TTY shell and the X8
 * Terminal both drive this same code; neither reimplements the command set. */
typedef struct {
    void (*puts)(const char *s, void *ctx);
    void (*putc)(int c, void *ctx);
    void (*clear)(void *ctx);
    void (*flush)(void *ctx);
    int  (*run)(const char *path, const xec_args_t *args, void *ctx);
    void *ctx;
} qnt_io_t;

void qnt_core_init(void);
void qnt_core_welcome(const qnt_io_t *io);
void qnt_core_prompt(const qnt_io_t *io);
/* Returns 1 when the shell should exit. */
int qnt_core_execute(const char *line, const qnt_io_t *io);
const char *qnt_core_cwd(void);
void qnt_core_set_cwd(const char *cwd);

#endif
