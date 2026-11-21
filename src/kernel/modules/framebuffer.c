#include "framebuffer.h"
#include "font.h"
#include "mouse.h"

static unsigned int *fb;
static int fb_w, fb_h, fb_pitch;
static int fb_valid;
static int cx, cy;
static int fb_rp = 16, fb_gp = 8, fb_bp = 0;

void console_init(void);

unsigned int fb_color(unsigned char r, unsigned char g, unsigned char b) {
    return ((unsigned int)r << fb_rp) | ((unsigned int)g << fb_gp) | ((unsigned int)b << fb_bp);
}

void framebuffer_init(struct vbe_result *vbe) {
    if (vbe && vbe->success) {
        fb = (unsigned int *)(unsigned long)vbe->fb_addr;
        fb_w = vbe->width;
        fb_h = vbe->height;
        fb_pitch = vbe->pitch;
        fb_rp = vbe->red_pos;
        fb_gp = vbe->green_pos;
        fb_bp = vbe->blue_pos;
        fb_valid = 1;
    } else {
        /* Keep a plausible geometry for the normal console, but do not let
         * the panic writer touch this guessed address: it may not be mapped. */
        fb = (unsigned int *)0xFD000000;
        fb_w = 1920;
        fb_h = 1080;
        fb_pitch = 1920 * 4;
        fb_rp = 16;
        fb_gp = 8;
        fb_bp = 0;
        fb_valid = 0;
    }
    cx = 0;
    cy = 0;
    init_font();
    console_init();
}

void fb_putpixel(int x, int y, unsigned int color) {
    if (x >= 0 && x < fb_w && y >= 0 && y < fb_h)
        fb[y * (fb_pitch / 4) + x] = color;
}

unsigned int fb_getpixel(int x, int y) {
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return 0;
    return fb[y * (fb_pitch / 4) + x];
}

int fb_ready(void) {
    return fb != 0;
}

/* True only when VBE gave us a real, mapped linear framebuffer.  Panic uses
 * this instead of fb_ready() so a failed VBE setup never causes a write to a
 * guessed address. */
int fb_usable(void) {
    return fb_valid && fb != 0;
}

int fb_width(void) {
    return fb_w;
}

int fb_height(void) {
    return fb_h;
}

void fb_putchar(int x, int y, char c, unsigned int fg, unsigned int bg) {
    unsigned char *g = F[(unsigned char)c];
    for (int r = 0; r < 16; r++) {
        unsigned char bits = g[r];
        for (int col = 0; col < 8; col++)
            fb_putpixel(x + col, y + r, (bits & (0x80 >> col)) ? fg : bg);
    }
}

void fb_drawstr(int x, int y, const char *s, unsigned int fg, unsigned int bg) {
    while (*s) {
        fb_putchar(x, y, *s, fg, bg);
        x += 8;
        s++;
    }
}

void fb_clear(unsigned int color) {
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * (fb_pitch / 4) + x] = color;
    cx = 0;
    cy = 0;
}

void fb_rect(int x, int y, int w, int h, unsigned int color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= fb_w || y >= fb_h || w <= 0 || h <= 0) return;
    if (x + w > fb_w) w = fb_w - x;
    if (y + h > fb_h) h = fb_h - y;

    for (int row = 0; row < h; row++) {
        unsigned int *dst = fb + (y + row) * (fb_pitch / 4) + x;
        for (int col = 0; col < w; col++) dst[col] = color;
    }
}

void fb_blit_scaled_rgb(const unsigned char *rgb, int src_w, int src_h) {
    if (!fb_valid || !fb || !rgb || src_w <= 0 || src_h <= 0) return;

    for (int y = 0; y < fb_h; y++) {
        int sy = (int)(((long long)y * src_h) / fb_h);
        const unsigned char *src = rgb + (long long)sy * src_w * 3;
        unsigned int *dst = fb + (long long)y * (fb_pitch / 4);
        for (int x = 0; x < fb_w; x++) {
            int sx = (int)(((long long)x * src_w) / fb_w);
            unsigned char r = src[sx * 3];
            unsigned char g = src[sx * 3 + 1];
            unsigned char b = src[sx * 3 + 2];
            dst[x] = ((unsigned int)r << fb_rp) |
                     ((unsigned int)g << fb_gp) |
                     ((unsigned int)b << fb_bp);
        }
    }
}

void fb_printk(const char *s) {
    while (*s) {
        if (*s == '\n') {
            cx = 0;
            cy += 16;
        } else {
            fb_putchar(cx, cy, *s, 0x00FFFFFF, 0x00000000);
            cx += 8;
            if (cx >= fb_w) {
                cx = 0;
                cy += 16;
            }
        }
        s++;
    }
}

static int cursor_visible = 1;
static unsigned int cursor_blink_ticks;
static int gui_depth;

/* Optional raw TTY capture.  X8's Terminal sets this around a qnt.xec call so
 * external commands such as lid.xec print into the window instead of the
 * hidden text console. */
static char *capture_buf;
static unsigned int capture_cap;
static unsigned int capture_len;

static void capture_bytes(const char *s, unsigned int n) {
    if (!capture_buf || !capture_cap) return;
    for (unsigned int i = 0; i < n; i++) {
        if (capture_len >= capture_cap) return;
        capture_buf[capture_len++] = s[i];
    }
}

static void capture_char(int c) {
    char byte = (char)(unsigned char)c;
    capture_bytes(&byte, 1);
}

void fb_capture_set(void *buf, unsigned int cap) {
    capture_buf = (char *)buf;
    capture_cap = cap;
    capture_len = 0;
}

unsigned int fb_capture_len(void) {
    return capture_len;
}

/* ------------------------------------------------------------------ */
/* Console: a cell grid rendered through the same font/framebuffer.    */
/* Row 0 is reserved for the top bar.                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char ch;
    unsigned char fg;
    unsigned char bg;
    unsigned char dirty;
} fb_cell_t;

#define MAX_COLS 240
#define MAX_ROWS 68

static fb_cell_t cells[MAX_ROWS][MAX_COLS];
static int cons_cols, cons_rows;
static int ccx, ccy;
static int cur_fg = C_GRAY, cur_bg = C_BLACK;
static int rainbow_index;
static int rainbow_on;
static int cur_col_valid;
static int cur_col, cur_row;

static const unsigned int palette[] = {
    0x00000000, /* BLACK   */
    0x00FFFFFF, /* WHITE   */
    0x00FF6666, /* RED     */
    0x0066FF66, /* GREEN   */
    0x006666FF, /* BLUE    */
    0x0066FFFF, /* CYAN    */
    0x00FF66FF, /* MAGENTA */
    0x00FFFF66, /* YELLOW  */
    0x00AAAAAA, /* GRAY    */
    0x00008080, /* TEAL    */
};

static const int rainbow_colors[] = {
    C_RED, C_YELLOW, C_GREEN, C_CYAN, C_BLUE, C_MAGENTA
};

static unsigned int color_of(int idx) {
    if (idx < 0 || idx >= (int)(sizeof(palette) / sizeof(palette[0]))) return 0x00FFFFFF;
    return palette[idx];
}

void console_init(void) {
    cons_cols = fb_w / 8;
    cons_rows = fb_h / 16;
    if (cons_cols > MAX_COLS) cons_cols = MAX_COLS;
    if (cons_rows > MAX_ROWS) cons_rows = MAX_ROWS;
    if (cons_cols < 1) cons_cols = 1;
    if (cons_rows < 2) cons_rows = 2;

    for (int r = 0; r < cons_rows; r++) {
        for (int c = 0; c < cons_cols; c++) {
            cells[r][c].ch = ' ';
            cells[r][c].fg = C_BLACK;
            cells[r][c].bg = C_WHITE;
            cells[r][c].dirty = 1;
        }
    }
    ccx = 0;
    ccy = 1;
    cur_col_valid = 0;
    rainbow_on = 0;
    rainbow_index = 0;
    fb_flush();
}

int fb_cols(void) { return cons_cols; }
int fb_rows(void) { return cons_rows; }

void fb_setcolor(int fg, int bg) {
    if (fg >= 0 && fg <= C_TEAL) cur_fg = fg;
    if (bg >= 0 && bg <= C_TEAL) cur_bg = bg;
    if (fg == C_RAINBOW) {
        rainbow_on = 1;
        rainbow_index = 0;
    } else if (fg >= 0 && fg <= C_TEAL) {
        rainbow_on = 0;
    }
}

void fb_setxy(int col, int row) {
    if (col < 0) col = 0;
    if (col >= cons_cols) col = cons_cols - 1;
    if (row < 0) row = 0;
    if (row >= cons_rows) row = cons_rows - 1;
    ccx = col;
    ccy = row;
}

unsigned int fb_getxy(void) {
    return (unsigned int)ccx | ((unsigned int)ccy << 16);
}

void fb_scroll(void) {
    for (int r = 1; r < cons_rows - 1; r++) {
        for (int c = 0; c < cons_cols; c++) {
            cells[r][c] = cells[r + 1][c];
            cells[r][c].dirty = 1;
        }
    }
    for (int c = 0; c < cons_cols; c++) {
        cells[cons_rows - 1][c].ch = ' ';
        cells[cons_rows - 1][c].fg = cur_fg;
        cells[cons_rows - 1][c].bg = cur_bg;
        cells[cons_rows - 1][c].dirty = 1;
    }
    ccy = cons_rows - 1;
}

static void advance(void) {
    ccx++;
    if (ccx >= cons_cols) {
        ccx = 0;
        ccy++;
    }
    if (ccy >= cons_rows) {
        fb_scroll();
    }
}

static void putc_inner(int c) {
    if (c == '\n') {
        ccx = 0;
        ccy++;
        if (ccy >= cons_rows) fb_scroll();
        return;
    }
    if (c == '\r') {
        ccx = 0;
        return;
    }
    if (c == '\b') {
        if (ccx > 0) ccx--;
        return;
    }
    if (c == '\t') {
        for (int i = 0; i < 4; i++) putc_inner(' ');
        return;
    }

    int fg = cur_fg;
    if (rainbow_on) {
        fg = rainbow_colors[rainbow_index % (int)(sizeof(rainbow_colors) / sizeof(rainbow_colors[0]))];
        rainbow_index++;
    }

    cells[ccy][ccx].ch = (unsigned char)c;
    cells[ccy][ccx].fg = (unsigned char)fg;
    cells[ccy][ccx].bg = (unsigned char)cur_bg;
    cells[ccy][ccx].dirty = 1;
    advance();
}

void fb_putc(int c) {
    /* While a GUI app is capturing the TTY, its output belongs only in the
     * capture buffer.  Writing the hidden console as well would replay every
     * prompt on the text screen as soon as the GUI exits. */
    if (capture_buf && capture_cap) {
        capture_char(c);
        return;
    }
    putc_inner(c);
}

static int color_index(const char *name, int len) {
    static const struct { const char *name; int val; } names[] = {
        { "BLACK", C_BLACK }, { "WHITE", C_WHITE }, { "RED", C_RED },
        { "GREEN", C_GREEN }, { "BLUE", C_BLUE }, { "CYAN", C_CYAN },
        { "MAGENTA", C_MAGENTA }, { "YELLOW", C_YELLOW }, { "GRAY", C_GRAY },
        { "TEAL", C_TEAL }, { "RAINBOW", C_RAINBOW },
    };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        const char *n = names[i].name;
        int j = 0;
        while (j < len && n[j] && n[j] == name[j]) j++;
        if (j == len && n[j] == '\0') return names[i].val;
    }
    return -1;
}

void fb_puts(const char *s) {
    if (!s) return;

    unsigned int n = 0;
    while (s[n]) n++;
    if (capture_buf && capture_cap) {
        capture_bytes(s, n);
        return;
    }

    while (*s) {
        if (s[0] == '\\' && s[1] == 'c' && s[2] == ':') {
            const char *p = s + 3;
            int len = 0;
            while (p[len] && p[len] != '|' && len < 16) len++;
            if (p[len] == '|') {
                int idx = color_index(p, len);
                if (idx >= 0) fb_setcolor(idx, cur_bg);
                s = p + len + 1;
                continue;
            }
        }
        putc_inner((unsigned char)*s++);
    }
    fb_flush();
}

void fb_setcell(int col, int row, int ch, int fg, int bg) {
    if (col < 0 || col >= cons_cols || row < 0 || row >= cons_rows) return;
    cells[row][col].ch = (unsigned char)ch;
    cells[row][col].fg = (unsigned char)fg;
    cells[row][col].bg = (unsigned char)bg;
    cells[row][col].dirty = 1;
}

void fb_clearline(int row) {
    if (row < 0 || row >= cons_rows) return;
    for (int c = 0; c < cons_cols; c++) {
        cells[row][c].ch = ' ';
        cells[row][c].fg = (unsigned char)cur_fg;
        cells[row][c].bg = (unsigned char)cur_bg;
        cells[row][c].dirty = 1;
    }
}

void fb_console_clear(void) {
    for (int r = 1; r < cons_rows; r++) {
        for (int c = 0; c < cons_cols; c++) {
            cells[r][c].ch = ' ';
            cells[r][c].fg = (unsigned char)cur_fg;
            cells[r][c].bg = (unsigned char)cur_bg;
            cells[r][c].dirty = 1;
        }
    }
    ccx = 0;
    ccy = 1;
    fb_flush();
}

void fb_gui_enter(void) {
    if (gui_depth++ == 0) {
        cursor_visible = 0;
    }
}

void fb_gui_leave(void) {
    if (gui_depth <= 0) return;
    if (--gui_depth != 0) return;

    cursor_visible = 1;
    cur_col_valid = 0;
    for (int r = 0; r < cons_rows; r++)
        for (int c = 0; c < cons_cols; c++)
            cells[r][c].dirty = 1;
    fb_flush();
}

int fb_gui_active(void) {
    return gui_depth > 0;
}

static void render_cell(int col, int row) {
    fb_cell_t *cell = &cells[row][col];
    fb_putchar(col * 8, row * 16, (char)cell->ch,
               color_of(cell->fg), color_of(cell->bg));
    cell->dirty = 0;
}

/* The cursor fills exactly one character cell. */
static void draw_cursor_cell(void) {
    if (ccx < 0 || ccx >= cons_cols || ccy < 0 || ccy >= cons_rows) return;
    unsigned int c = color_of(cells[ccy][ccx].fg);
    int x = ccx * 8;
    int y = ccy * 16;
    for (int r = 0; r < 16; r++)
        for (int col = 0; col < 8; col++)
            fb_putpixel(x + col, y + r, c);
}

void fb_flush(void) {
    if (gui_depth > 0) return;

    for (int r = 0; r < cons_rows; r++) {
        for (int c = 0; c < cons_cols; c++) {
            if (cells[r][c].dirty) render_cell(c, r);
        }
    }

    if (cur_col_valid) render_cell(cur_col, cur_row);

    if (cursor_visible) draw_cursor_cell();

    cur_col = ccx;
    cur_row = ccy;
    cur_col_valid = 1;
}

/* Called once per PIT tick (1 kHz); toggles the cursor every 500 ms. */
void fb_cursor_blink(void) {
    if (gui_depth > 0) return;
    if (++cursor_blink_ticks < 500) return;
    cursor_blink_ticks = 0;
    cursor_visible = !cursor_visible;

    if (cur_col_valid) render_cell(cur_col, cur_row);
    if (cursor_visible) draw_cursor_cell();
}
