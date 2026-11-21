#include "time.h"

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static volatile uint64_t ticks;
static volatile uint64_t idle_ticks;
static uint64_t mark_ticks;
static uint64_t mark_idle;
static uint32_t cached_usage;

void time_init(void) {
    /* PIT channel 0, rate generator, ~1000 Hz (divisor 1193). */
    outb(0x43, 0x34);
    outb(0x40, 0xA9);
    outb(0x40, 0x04);
}

void time_tick(void) {
    ticks++;
}

uint64_t time_ticks(void) {
    return ticks;
}

uint64_t time_uptime_ms(void) {
    return ticks;
}

void time_idle_tick(void) {
    idle_ticks++;
}

uint32_t time_cpu_usage(void) {
    uint64_t now = ticks;
    uint64_t idle = idle_ticks;
    uint64_t dt = now - mark_ticks;

    if (dt < 100) return cached_usage;

    uint64_t di = idle - mark_idle;
    uint32_t usage = 100 - (uint32_t)((di * 100) / dt);
    if (usage > 99) usage = 99;

    cached_usage = usage;
    mark_ticks = now;
    mark_idle = idle;
    return usage;
}

void time_sleep(int ms) {
    uint64_t end = ticks + (uint64_t)ms;
    while (ticks < end) __asm__ volatile ("hlt");
}
