#include "xecabi.h"
#include "inflate.h"

#define FILE_CAP    (3 * 1024 * 1024)
#define PAYLOAD_CAP (4 * 1024 * 1024)
#define PCM_CAP     (2 * 1024 * 1024)

static unsigned char file_buf[FILE_CAP];
static unsigned char payload[PAYLOAD_CAP];
static int16_t pcm[PCM_CAP];

static const int16_t IMA_STEP[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,
    73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,
    408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,
    1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,
    7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,
    22385,24623,27086,29794,32767
};
static const int8_t IMA_INDEX[16] = {
    -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8
};

static void full_path(const char *name, const char *cwd, char *out, unsigned int cap) {
    unsigned int n = 0;
    if (name[0] == '/') {
        while (name[n] && n < cap - 1) { out[n] = name[n]; n++; }
    } else {
        unsigned int i = 0;
        while (cwd && cwd[i] && n < cap - 1) out[n++] = cwd[i++];
        if (n > 0 && out[n - 1] != '/' && n < cap - 1) out[n++] = '/';
        i = 0;
        while (name[i] && n < cap - 1) out[n++] = name[i++];
    }
    out[n] = '\0';
}

static int has_slash(const char *name) {
    for (unsigned int i = 0; name[i]; i++) {
        if (name[i] == '/') return 1;
    }
    return 0;
}

static int read_file(const char *name, const char *cwd, const void **raw,
                     uint32_t *size, char *path, unsigned int cap) {
    full_path(name, cwd, path, cap);
    if (fs_read(path, raw, size) == 0) return 0;
    if (name[0] == '/') return -1;

    unsigned int n = 0;
    const char *prefix = has_slash(name) ? "" : "media/";
    for (unsigned int i = 0; prefix[i] && n < cap - 1; i++) path[n++] = prefix[i];
    for (unsigned int i = 0; name[i] && n < cap - 1; i++) path[n++] = name[i];
    path[n] = '\0';
    return fs_read(path, raw, size);
}

static const char *find_key(const char *j, const char *key) {
    static char needle[40];
    unsigned int n = 0;
    needle[n++] = '"';
    for (unsigned int i = 0; key[i] && n < sizeof(needle) - 2; i++) needle[n++] = key[i];
    needle[n++] = '"';
    needle[n] = '\0';

    const char *p = str_str(j, needle);
    if (!p) return 0;
    p += n;
    while (*p == ' ' || *p == '\t' || *p == ':') p++;
    return p;
}

static int json_int(const char *j, const char *key, int fallback) {
    const char *p = find_key(j, key);
    if (!p) return fallback;
    if (*p == '[') p++;
    while (*p == ' ' || *p == '\t') p++;
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    int v = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
    return sign * v;
}

static void json_array_ints(const char *j, const char *key, int *values, int count) {
    const char *p = find_key(j, key);
    for (int i = 0; i < count; i++) values[i] = 0;
    if (!p) return;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '[') p++;
    for (int i = 0; i < count; i++) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        int sign = 1;
        if (*p == '-') { sign = -1; p++; }
        int v = 0;
        while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
        values[i] = sign * v;
        while (*p && *p != ',' && *p != ']') p++;
    }
}

static void json_string(const char *j, const char *key, char *out, unsigned int cap) {
    out[0] = '\0';
    const char *p = find_key(j, key);
    if (!p || *p != '"') return;
    p++;
    unsigned int n = 0;
    while (*p && *p != '"' && n < cap - 1) out[n++] = *p++;
    out[n] = '\0';
}

static void int_to_str(long v, char *out) {
    int n = 0;
    if (v == 0) { out[0] = '0'; out[1] = '\0'; return; }
    unsigned long u;
    if (v < 0) { out[0] = '-'; out[n = 1] = '\0'; u = (unsigned long)(-v); }
    else u = (unsigned long)v;
    char digits[24];
    int d = 0;
    while (u) { digits[d++] = (char)('0' + u % 10); u /= 10; }
    if (n == 1) {
        for (int i = 0; i < d; i++) out[1 + i] = digits[d - 1 - i];
        out[1 + d] = '\0';
    } else {
        for (int i = 0; i < d; i++) out[i] = digits[d - 1 - i];
        out[d] = '\0';
    }
}

static int ima_decode_channel(const unsigned char *packed, int seed,
                              uint32_t frames, unsigned int stride,
                              unsigned int first, int16_t *out) {
    if (!frames) return 0;
    int predictor = seed;
    int index = 0;
    out[0] = (int16_t)predictor;
    for (uint32_t f = 1; f < frames; f++) {
        uint32_t pos = (f - 1) * stride + first;
        int code = (packed[pos / 2] >> ((pos & 1) ? 4 : 0)) & 0x0F;
        int step = IMA_STEP[index];
        int diff = step >> 3;
        if (code & 4) diff += step;
        if (code & 2) diff += step >> 1;
        if (code & 1) diff += step >> 2;
        predictor += (code & 8) ? -diff : diff;
        if (predictor > 32767) predictor = 32767;
        if (predictor < -32768) predictor = -32768;
        index += IMA_INDEX[code & 15];
        if (index < 0) index = 0;
        if (index > 88) index = 88;
        out[f * stride] = (int16_t)predictor;
    }
    return 0;
}

static void decode_delta3(int16_t *samples, uint32_t frames, int channels) {
    for (int pass = 0; pass < 3; pass++) {
        for (int ch = 0; ch < channels; ch++) {
            int acc = 0;
            for (uint32_t f = 0; f < frames; f++) {
                acc += samples[f * channels + ch];
                acc = ((acc + 32768) & 0xFFFF) - 32768;
                samples[f * channels + ch] = (int16_t)acc;
            }
        }
    }
}

static int decode_adpcm(const unsigned char *raw, uint32_t raw_size,
                        const char *codec, const int *seeds,
                        uint32_t frames, int channels, int16_t **out) {
    uint32_t need = (frames - 1) * (uint32_t)channels;
    if (raw_size * 2 < need) return -1;

    if (str_eq(codec, "ima-adpcm-4bit-interleaved-v3")) {
        for (int ch = 0; ch < channels; ch++)
            ima_decode_channel(raw, seeds[ch], frames, (unsigned int)channels,
                               (unsigned int)ch, pcm + ch);
    } else {
        ima_decode_channel(raw, seeds[0], frames, 1, 0, pcm);
    }
    *out = pcm;
    return 0;
}

static int decode_basic(const unsigned char *raw, const char *dtype,
                        const char *transform, uint32_t frames, int channels,
                        int16_t **out) {
    uint32_t count = frames * (uint32_t)channels;
    if (str_eq(dtype, "i8")) {
        for (uint32_t i = 0; i < count; i++) pcm[i] = (int16_t)((int8_t)raw[i]) * 256;
    } else {
        for (uint32_t i = 0; i < count; i++) {
            int v = (int)(int16_t)((uint16_t)raw[i * 2] | ((uint16_t)raw[i * 2 + 1] << 8));
            pcm[i] = (int16_t)v;
        }
        if (str_eq(transform, "modular-delta3")) decode_delta3(pcm, frames, channels);
    }
    *out = pcm;
    return 0;
}

static void print_wave(const char *name, const char *header, int frames,
                       int rate, int channels, const int16_t *samples) {
    char num[24];
    tty_setcolor(C_GRAY, C_BLACK);
    tty_puts("\\c:WHITE|RPLAY3: ");
    tty_puts(name);
    tty_putc('\n');
    tty_puts("\\c:GRAY|  frames : "); int_to_str(frames, num); tty_puts(num); tty_putc('\n');
    tty_puts("\\c:GRAY|  rate   : "); int_to_str(rate, num); tty_puts(num); tty_putc('\n');
    tty_puts("\\c:GRAY|  channel: "); int_to_str(channels, num); tty_puts(num); tty_putc('\n');
    tty_puts("\\c:GRAY|  codec  : "); tty_puts(header); tty_putc('\n');

    int min = 32767, max = -32768;
    uint64_t sum = 0;
    uint32_t n = frames < 4096 ? (uint32_t)frames : 4096;
    for (uint32_t i = 0; i < n; i++) {
        int v = samples[i];
        if (v < min) min = v;
        if (v > max) max = v;
        sum += (uint64_t)(v * v);
    }
    tty_puts("\\c:GRAY|  range  : "); int_to_str(min, num); tty_puts(num); tty_puts(" .. ");
    int_to_str(max, num); tty_puts(num); tty_putc('\n');
    tty_puts("\\c:GRAY|  rms    : ");
    unsigned long rms = n ? (unsigned long)(sum / n) : 0;
    unsigned long r = 0, x = rms;
    while (x > 0) { x >>= 1; r++; }
    int_to_str((long)(r ? r : 0), num); tty_puts(num); tty_puts(" (bits)\n");

    tty_puts("\\c:GRAY|  wave   : ");
    for (int col = 0; col < 40; col++) {
        uint32_t start = (uint32_t)(col * (int)n / 40);
        uint32_t end = (uint32_t)((col + 1) * (int)n / 40);
        int peak = 0;
        for (uint32_t i = start; i < end; i++) {
            int a = samples[i] < 0 ? -samples[i] : samples[i];
            if (a > peak) peak = a;
        }
        int level = peak * 12 / 32768;
        for (int i = 0; i < level; i++) tty_putc('#');
        for (int i = level; i < 12; i++) tty_putc('.');
    }
    tty_putc('\n');
}

int xec_entry(const xec_args_t *args) {
    if (!args || args->argc < 2) {
        tty_puts("\\c:RED|usage: rplay <file.rplay3> [--all|--print]\n");
        return 1;
    }

    int print_only = 0, try_all = 0;
    for (unsigned int i = 2; i < args->argc; i++) {
        if (str_eq(args->argv[i], "--print")) print_only = 1;
        if (str_eq(args->argv[i], "--all")) try_all = 1;
    }

    char path[160];
    const void *raw = 0;
    uint32_t size = 0;
    if (read_file(args->argv[1], args->cwd, &raw, &size,
                  path, sizeof(path)) != 0 || size > FILE_CAP) {
        tty_puts("\\c:RED|rplay: cannot read ");
        tty_puts(path);
        tty_putc('\n');
        return 1;
    }
    mem_copy(file_buf, raw, size);

    if (size < 12 || file_buf[0] != 'R' || file_buf[1] != 'P' ||
        file_buf[2] != 'L' || file_buf[3] != 'A' || file_buf[4] != 'Y' ||
        file_buf[5] != '3' || file_buf[6] != 0 || file_buf[7] != 2) {
        tty_puts("\\c:RED|rplay: not an RPLAY3 file\n");
        return 1;
    }

    uint32_t hsize = *(uint32_t *)(file_buf + 8);
    if (hsize == 0 || hsize > size - 12) {
        tty_puts("\\c:RED|rplay: bad header size\n");
        return 1;
    }
    const char *header = (const char *)(file_buf + 12);
    const unsigned char *compressed = file_buf + 12 + hsize;
    uint32_t compressed_size = size - 12 - hsize;

    char container[32];
    json_string(header, "container", container, sizeof(container));
    int payload_len = -1;

    if (str_eq(container, "rplay3-raw")) {
        if (compressed_size > PAYLOAD_CAP) return 1;
        mem_copy(payload, compressed, compressed_size);
        payload_len = (int)compressed_size;
    } else if (str_eq(container, "rplay3-zlib")) {
        payload_len = wz_inflate(compressed, compressed_size, payload, PAYLOAD_CAP);
    } else {
        tty_puts("\\c:RED|rplay: container ");
        tty_puts(container);
        tty_puts(" needs rplay_prep.py\n");
        return 1;
    }
    if (payload_len < 0) {
        tty_puts("\\c:RED|rplay: decompress failed\n");
        return 1;
    }

    int frames = json_int(header, "frames", 0);
    int channels = json_int(header, "channels", 1);
    int rate = json_int(header, "rate", 8000);
    int ii_size = json_int(header, "ii_payload_bytes", 0);
    char codec[48], dtype[16], transform[24];
    json_string(header, "basic_codec", codec, sizeof(codec));
    json_string(header, "dtype", dtype, sizeof(dtype));
    json_string(header, "ii_transform", transform, sizeof(transform));

    if (frames <= 0 || channels < 1 || channels > 2 || ii_size > payload_len) {
        tty_puts("\\c:RED|rplay: bad audio header\n");
        return 1;
    }

    int16_t *samples = 0;
    if (str_eq(codec, "ima-adpcm-4bit-hex-rle-v2") ||
        str_eq(codec, "ima-adpcm-4bit-interleaved-v3")) {
        int seeds[2] = { 0, 0 };
        json_array_ints(header, "seed", seeds, channels > 1 ? 2 : 1);
        if (decode_adpcm(payload, (uint32_t)ii_size, codec, seeds,
                         (uint32_t)frames, channels, &samples) != 0) {
            tty_puts("\\c:RED|rplay: ADPCM decode failed\n");
            return 1;
        }
    } else {
        if (decode_basic(payload, dtype, transform, (uint32_t)frames,
                         channels, &samples) != 0) {
            tty_puts("\\c:RED|rplay: basic decode failed\n");
            return 1;
        }
    }

    uint32_t pcm_frames = (uint32_t)frames;

    if (print_only) {
        print_wave(args->argv[1], codec, frames, rate, channels, samples);
        return 0;
    }

    int played = 0;
    int probe = audio_probe();
    if (try_all) {
        if ((probe & AUDIO_BACKEND_PC) &&
            audio_play(AUDIO_BACKEND_PC, samples, pcm_frames,
                       (uint32_t)rate, (uint32_t)channels) == 0) {
            tty_puts("\\c:GREEN|rplay: playing via pc speaker\n");
            played = 1;
        }
        if (!played && (probe & AUDIO_BACKEND_SB) &&
            audio_play(AUDIO_BACKEND_SB, samples, pcm_frames,
                       (uint32_t)rate, (uint32_t)channels) == 0) {
            tty_puts("\\c:GREEN|rplay: playing via sound blaster\n");
            played = 1;
        }
    } else if ((probe & AUDIO_BACKEND_PC) &&
               audio_play(AUDIO_BACKEND_PC, samples, pcm_frames,
                          (uint32_t)rate, (uint32_t)channels) == 0) {
        tty_puts("\\c:GREEN|rplay: playing via pc speaker\n");
        played = 1;
    }

    if (!played) {
        tty_puts("\\c:YELLOW|no work\n");
        print_wave(args->argv[1], codec, frames, rate, channels, samples);
    }
    return 0;
}
