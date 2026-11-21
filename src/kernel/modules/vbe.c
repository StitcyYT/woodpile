#include <stdint.h>
#include "modules/vbe.h"

extern uint64_t vbe_result_fb_addr;
extern uint64_t vbe_result_mem_top;
extern uint8_t vbe_trace;
extern uint16_t best_mode;
extern uint64_t best_pixels;
extern uint16_t vbe_result_width;
extern uint16_t vbe_result_height;
extern uint16_t vbe_result_pitch;
extern uint8_t  vbe_result_bpp;
extern uint8_t  vbe_result_success;
extern uint8_t  vbe_result_red_pos;
extern uint8_t  vbe_result_green_pos;
extern uint8_t  vbe_result_blue_pos;
extern uint16_t vbe_request_width;
extern uint16_t vbe_request_height;
extern uint8_t  vbe_exact;

#ifndef VBE_DEFAULT_WIDTH
#define VBE_DEFAULT_WIDTH 1920
#endif
#ifndef VBE_DEFAULT_HEIGHT
#define VBE_DEFAULT_HEIGHT 1080
#endif

static uint16_t wanted_width  = VBE_DEFAULT_WIDTH;
static uint16_t wanted_height = VBE_DEFAULT_HEIGHT;

extern void vbe_enter_trampoline(void);

void vbe_get_resolution(uint16_t *width, uint16_t *height) {
    if (width) *width = wanted_width;
    if (height) *height = wanted_height;
}

int vbe_set_resolution(uint16_t width, uint16_t height, struct vbe_result *res) {
    wanted_width  = width  ? width  : VBE_DEFAULT_WIDTH;
    wanted_height = height ? height : VBE_DEFAULT_HEIGHT;

    vbe_request_width  = wanted_width;
    vbe_request_height = wanted_height;

    return vbe_init(res);
}

int vbe_exact_match(void) {
    return vbe_exact ? 1 : 0;
}

int vbe_init(struct vbe_result *res) {
    vbe_enter_trampoline();

    res->fb_addr = vbe_result_fb_addr;
    res->width   = vbe_result_width;
    res->height  = vbe_result_height;
    res->pitch   = vbe_result_pitch;
    res->bpp     = vbe_result_bpp;
    res->success = vbe_result_success;
    res->red_pos   = vbe_result_red_pos;
    res->green_pos = vbe_result_green_pos;
    res->blue_pos  = vbe_result_blue_pos;

    return res->success ? 0 : -1;
}

uint64_t vbe_mem_top(void) {
    return vbe_result_mem_top;
}

uint8_t vbe_trace_code(void) {
    return vbe_trace;
}

uint16_t vbe_best_mode_code(void) {
    return best_mode;
}

uint64_t vbe_best_pixels_code(void) {
    return best_pixels;
}
