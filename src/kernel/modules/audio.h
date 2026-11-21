#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

enum {
    AUDIO_BACKEND_PC = 1,
    AUDIO_BACKEND_SB = 2,
};

/* Bitmask of backends that responded to a probe. */
int audio_probe(void);

/* Play interleaved signed 16-bit PCM. Returns 0 on success, -1 on failure. */
int audio_play(int backend, const void *pcm, uint32_t frames,
               uint32_t rate, uint32_t channels);

#endif
