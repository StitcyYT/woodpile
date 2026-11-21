#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>

/* Normal panic: styled panel on the framebuffer (or VGA text) plus serial. */
void panic(const char *reason, uint32_t code, uint64_t dump);

/* Severe panic: serial-first minimal dump; never touches the framebuffer.
 * Used for double faults and as the fallback if the full writer faults. */
void panic_severe(const char *reason, uint32_t code, uint64_t dump);

#endif
