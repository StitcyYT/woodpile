#include "audio.h"
#include "time.h"

static inline unsigned char inb(unsigned short port) {
    unsigned char v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    /* QEMU TCG updates the virtual TSC when it leaves a translation block.
     * A device read forces that exit, otherwise a tight rdtsc loop can spin
     * forever with a frozen counter. */
    (void)inb(0x61);
    return ((uint64_t)hi << 32) | lo;
}

/* Calibrate into a local: the old global sat at the very end of .bss and was
 * overwritten during shell idle, which made the playback delay enormous. */
static uint64_t calibrate_tsc(void) {
    uint64_t t0 = rdtsc();
    uint64_t tick = time_ticks();
    while (time_ticks() - tick < 50) { }
    uint64_t t1 = rdtsc();
    uint64_t hz = (t1 - t0) * 20;        /* 50 ms => x20 for one second */
    return hz ? hz : 1000000000ULL;
}

static void delay_tsc(uint64_t n) {
    uint64_t end = rdtsc() + n;
    while (rdtsc() < end) { }
}

/* PC speaker: pulse-width modulate the data bit at the sample rate. */
static int play_pc(const void *pcm, uint32_t frames, uint32_t rate, uint32_t channels) {
    if (!pcm || !frames || !rate || !channels) return -1;
    uint64_t hz = calibrate_tsc();

    uint64_t period = hz / rate;
    if (!period) period = 1;

    unsigned char port = inb(0x61);
    unsigned char gate = (unsigned char)((port | 1) & ~2);  /* gate on, data off */
    outb(0x61, gate);

    const int16_t *s = (const int16_t *)pcm;
    for (uint32_t i = 0; i < frames; i++) {
        int32_t v = s[i * channels];
        if (channels > 1) {
            int32_t sum = v;
            for (uint32_t c = 1; c < channels; c++) sum += s[i * channels + c];
            v = sum / (int32_t)channels;
        }

        uint64_t duty = ((uint64_t)(int32_t)(v + 32768) * period) / 65536ULL;
        outb(0x61, (unsigned char)(gate | 2));
        delay_tsc(duty);
        outb(0x61, gate);
        delay_tsc(period - duty);
    }

    outb(0x61, port & (unsigned char)~3);
    return 0;
}

static int sb_detect(uint16_t base) {
    outb((unsigned short)(base + 6), 1);
    for (volatile int i = 0; i < 100; i++) { }
    outb((unsigned short)(base + 6), 0);
    for (volatile int i = 0; i < 100; i++) { }
    return inb((unsigned short)(base + 0x0A)) == 0xAA ? 1 : 0;
}

int audio_probe(void) {
    int mask = 0;
    /* the PC speaker is always present on PC-compatible hardware */
    (void)inb(0x61);
    mask |= AUDIO_BACKEND_PC;
    if (sb_detect(0x220) || sb_detect(0x240)) mask |= AUDIO_BACKEND_SB;
    return mask;
}

int audio_play(int backend, const void *pcm, uint32_t frames,
               uint32_t rate, uint32_t channels) {
    if (backend == AUDIO_BACKEND_PC) return play_pc(pcm, frames, rate, channels);
    if (backend == AUDIO_BACKEND_SB) return -1;   /* DMA playback not implemented yet */
    return -1;
}
