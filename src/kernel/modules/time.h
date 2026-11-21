#ifndef TIME_H
#define TIME_H

#include <stdint.h>

void time_init(void);
void time_sleep(int ms);

/* Periodic PIT tick support (1 kHz). */
void time_tick(void);
uint64_t time_ticks(void);
uint64_t time_uptime_ms(void);

/* CPU-usage accounting: the shell marks idle time while waiting for keys. */
void time_idle_tick(void);
uint32_t time_cpu_usage(void);

#endif
