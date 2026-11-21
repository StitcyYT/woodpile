#ifndef VBE_H
#define VBE_H

#include <stdint.h>

struct vbe_result {
    uint64_t fb_addr;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint8_t  bpp;
    uint8_t  success;
    uint8_t  red_pos;
    uint8_t  green_pos;
    uint8_t  blue_pos;
};

int vbe_init(struct vbe_result *res);
void vbe_get_resolution(uint16_t *width, uint16_t *height);
int vbe_set_resolution(uint16_t width, uint16_t height, struct vbe_result *res);
int vbe_exact_match(void);
uint64_t vbe_mem_top(void);
uint8_t vbe_trace_code(void);
uint16_t vbe_best_mode_code(void);
uint64_t vbe_best_pixels_code(void);

#endif
