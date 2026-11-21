#include "initramfs.h"
#include "serial.h"

typedef struct {
    uint32_t count;
    initramfs_file_t files[64];
    uint8_t initialized;
} initramfs_state_t;

static initramfs_state_t state;

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void initramfs_init(void) {
    state.initialized = 0;
    state.count = 0;

    extern uint64_t initramfs_ptr;
    extern uint64_t initramfs_size;
    const uint8_t *base = (const uint8_t *)initramfs_ptr;
    uint32_t total = (uint32_t)initramfs_size;

    if (!base || total < 8) {
        serial_puts("[wpfs] no initramfs found\n");
        return;
    }

    uint32_t magic = read_u32(base);
    if (magic != 0x53465057) {
        serial_puts("[wpfs] bad magic\n");
        return;
    }

    state.count = read_u32(base + 4);
    serial_puts("[wpfs] found ");
    serial_puthex(state.count);
    serial_puts(" files\n");

    const uint8_t *p = base + 8;
    const uint8_t *end = base + total;

    for (uint32_t i = 0; i < state.count && i < 64; i++) {
        if (p + 4 > end) break;
        uint32_t nlen = read_u32(p); p += 4;
        if (p + nlen > end) break;
        state.files[i].name = (const char *)p;
        state.files[i].name_len = nlen;
        p += nlen;
        if (p + 4 > end) break;
        uint32_t dlen = read_u32(p); p += 4;
        if (p + dlen > end) break;
        state.files[i].data = p;
        state.files[i].data_len = dlen;
        p += dlen;
    }

    state.initialized = 1;
}

uint32_t initramfs_count(void) {
    return state.count;
}

initramfs_file_t *initramfs_get(uint32_t index) {
    if (!state.initialized || index >= state.count) return 0;
    return &state.files[index];
}

static uint32_t my_strlen(const char *s) {
    uint32_t n = 0;
    while (*s++) n++;
    return n;
}

static int my_memcmp(const void *a, const void *b, uint32_t n) {
    const uint8_t *x = a, *y = b;
    for (uint32_t i = 0; i < n; i++) {
        if (x[i] != y[i]) return x[i] - y[i];
    }
    return 0;
}

initramfs_file_t *initramfs_find(const char *name) {
    if (!state.initialized || !name) return 0;
    while (*name == '/') name++;      /* "/sys32/run64.co" == "sys32/run64.co" */
    uint32_t nlen = my_strlen(name);
    for (uint32_t i = 0; i < state.count; i++) {
        if (state.files[i].name_len == nlen &&
            my_memcmp(state.files[i].name, name, nlen) == 0)
            return &state.files[i];
    }
    return 0;
}
