#ifndef XECABI_H
#define XECABI_H

#include <stdint.h>

typedef struct {
    uint32_t argc;
    uint32_t reserved;
    const char *argv[16];
    const char *cwd;
} xec_args_t;

enum {
    C_BLACK = 0, C_WHITE, C_RED, C_GREEN, C_BLUE, C_CYAN,
    C_MAGENTA, C_YELLOW, C_GRAY, C_TEAL, C_RAINBOW
};

void tty_clear(void);
void tty_putc(int c);
void tty_puts(const char *s);
void tty_setcolor(int fg, int bg);
void tty_setxy(int col, int row);
unsigned int tty_getxy(void);
int tty_cols(void);
int tty_rows(void);
void tty_setcell(int col, int row, int ch, int fg, int bg);
void tty_clearline(int row);
void tty_flush(void);
void tty_scroll(void);

int fs_read(const char *path, const void **data, uint32_t *size);
int fs_write(const char *path, const void *data, uint32_t size);
int fs_remove(const char *path);
int fs_list(const char *dir, char *buf, uint32_t bufsize);
int fs_is_dir(const char *path);

int xec_run(const char *path, const xec_args_t *args);

void serial_puts(const char *s);
void serial_puthex(uint64_t v);
void time_sleep(int ms);
uint64_t time_ticks(void);
uint32_t time_cpu_usage(void);
uint64_t time_uptime_ms(void);

int kbd_getline(char *buf, int max);
int kbd_getc(void);

int audio_probe(void);
int audio_play(int backend, const void *pcm, uint32_t frames,
               uint32_t rate, uint32_t channels);

/* X8 graphics/input ABI. These suspend the text console while active. */
void gfx_enter(void);
void gfx_leave(void);
int gfx_width(void);
int gfx_height(void);
unsigned int gfx_rgb(unsigned char r, unsigned char g, unsigned char b);
void gfx_clear(unsigned int color);
void gfx_pixel(int x, int y, unsigned int color);
unsigned int gfx_getpixel(int x, int y);
void gfx_rect(int x, int y, int w, int h, unsigned int color);
void gfx_text(int x, int y, const char *s, unsigned int fg, unsigned int bg);
void mouse_state(int *x, int *y, int *buttons);
int kbd_scancode(void);

enum {
    AUDIO_BACKEND_PC = 1,
    AUDIO_BACKEND_SB = 2,
};

/* Small helpers (no libc). */
unsigned int str_len(const char *s);
int str_cmp(const char *a, const char *b);
int str_eq(const char *a, const char *b);
const char *str_str(const char *haystack, const char *needle);
void str_copy(char *dst, const char *src);
void mem_copy(void *dst, const void *src, unsigned int n);
void mem_set(void *dst, int c, unsigned int n);
void num_to_str(unsigned long v, char *out);

#endif
