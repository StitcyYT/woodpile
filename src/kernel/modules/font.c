#include "font.h"

unsigned char F[256][16];

extern unsigned char font_data_start[];
extern unsigned char font_data_end[];

void init_font(void) {
    unsigned char *src = font_data_start;
    for (int c = 0; c < 256; c++)
        for (int r = 0; r < 16; r++)
            F[c][r] = src[c * 16 + r];
}
