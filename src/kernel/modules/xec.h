#ifndef XEC_H
#define XEC_H

#include <stdint.h>

/* Arguments handed to every .xec entry point. */
typedef struct {
    uint32_t argc;
    uint32_t reserved;
    const char *argv[16];
    const char *cwd;
} xec_args_t;

int xec_run(const char *path, const xec_args_t *args);

#endif
