#include "x8_cursor.h"

static unsigned int scratch_color[X8_CURSOR_PIXELS];
static unsigned char scratch_alpha[X8_CURSOR_PIXELS];

static int saved_active;
static int saved_x, saved_y, saved_w, saved_h;
static unsigned int saved_pixels[X8_CURSOR_PIXELS];
static unsigned int drawn_pixels[X8_CURSOR_PIXELS];
/* Only pixels with alpha > 0 were actually overwritten; never touch the
 * rest when hiding, or we erase whatever the app drew there. */
static unsigned char saved_mask[X8_CURSOR_PIXELS];

/* Avoid restoring and repainting an identical cursor every frame.  That
 * hide/draw churn is what makes a stationary pointer look jittery. */
static x8_cursor_t *saved_cur;
static int saved_frame;
static int saved_draw_x, saved_draw_y;
static int saved_hot_x, saved_hot_y;

/* Framebuffer channel layout for alpha-blending the CUR's own pixels. */
static int blend_ready;
static int sh_r, sh_g, sh_b;

static int shift_of(unsigned int v) {
    int s = 0;
    while (v && !(v & 1)) { v >>= 1; s++; }
    return s;
}

static void blend_init(void) {
    if (blend_ready) return;
    sh_r = shift_of(gfx_rgb(255, 0, 0));
    sh_g = shift_of(gfx_rgb(0, 255, 0));
    sh_b = shift_of(gfx_rgb(0, 0, 255));
    blend_ready = 1;
}

static unsigned int blend_pixel(unsigned int bg, unsigned int fg, int a) {
    if (a >= 255) return fg;
    if (a <= 0) return bg;
    int br = (int)((bg >> sh_r) & 0xFF);
    int bgc = (int)((bg >> sh_g) & 0xFF);
    int bb = (int)((bg >> sh_b) & 0xFF);
    int fr = (int)((fg >> sh_r) & 0xFF);
    int fgc = (int)((fg >> sh_g) & 0xFF);
    int fb = (int)((fg >> sh_b) & 0xFF);
    int ia = 255 - a;
    int r = (fr * a + br * ia) / 255;
    int g = (fgc * a + bgc * ia) / 255;
    int b = (fb * a + bb * ia) / 255;
    return gfx_rgb((unsigned char)r, (unsigned char)g, (unsigned char)b);
}

/* Check only the pixels we actually painted.  If any no longer matches what
 * we drew, the app repainted behind us and a stale cursor would be left. */
static int cursor_pixels_intact(void) {
    if (!saved_active || saved_w <= 0 || saved_h <= 0) return 0;
    int total = saved_w * saved_h;
    for (int k = 0; k < total; k++) {
        if (!saved_mask[k]) continue;
        if (gfx_getpixel(saved_x + k % saved_w,
                         saved_y + k / saved_w) != drawn_pixels[k])
            return 0;
    }
    return 1;
}

static unsigned short rd16(const unsigned char *p) {
    return (unsigned short)(p[0] | (p[1] << 8));
}

static unsigned int rd32(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static int mask_shift(unsigned int mask) {
    int shift = 0;
    while (mask && !(mask & 1)) { mask >>= 1; shift++; }
    return shift;
}

static int mask_bits(unsigned int mask) {
    int bits = 0;
    while (mask) { bits++; mask >>= 1; }
    return bits;
}

static int extract_channel(unsigned int value, unsigned int mask) {
    if (!mask) return 0;
    int shift = mask_shift(mask);
    int bits = mask_bits(mask);
    unsigned int v = (value & mask) >> shift;
    if (bits >= 8) return (int)(v & 0xFF);
    int maxv = (1 << bits) - 1;
    if (maxv <= 0) return 0;
    return (int)((v * 255) / (unsigned int)maxv);
}

static int decode_dib(const unsigned char *dib, unsigned int size,
                      int *out_w, int *out_h,
                      unsigned int *out_color, unsigned char *out_alpha) {
    if (size < 40 || rd32(dib) < 40) return -1;

    int hdr = (int)rd32(dib);
    int w = (int)rd32(dib + 4);
    int full_h = (int)rd32(dib + 8);
    int h = full_h > 0 ? full_h / 2 : -full_h / 2;
    int bpp = rd16(dib + 14);
    unsigned int comp = rd32(dib + 16);
    unsigned int used = rd32(dib + 32);

    if (w <= 0 || h <= 0 || w > X8_CURSOR_MAX_W || h > X8_CURSOR_MAX_H)
        return -1;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32)
        return -1;
    if (comp != 0 && comp != 3) return -1;

    int off = hdr;
    unsigned int rm = 0, gm = 0, bm = 0, am = 0;
    if (comp == 3) {
        if ((unsigned int)off + 12 > size) return -1;
        rm = rd32(dib + off);
        gm = rd32(dib + off + 4);
        bm = rd32(dib + off + 8);
        am = 0xFF000000u;
        off += 12;
    } else if (bpp == 32) {
        /* DIB stores pixels as R,G,B,A bytes; rd32() builds 0xAABBGGRR,
         * so R is the low byte, alpha the high byte. */
        rm = 0x000000FFu; gm = 0x0000FF00u; bm = 0x00FF0000u;
        am = 0xFF000000u;
    } else if (bpp == 16) {
        rm = 0x00007C00u; gm = 0x000003E0u; bm = 0x0000001Fu; am = 0;
    }

    int palette_count = 0;
    if (bpp <= 8) {
        palette_count = used ? (int)used : (1 << bpp);
        if (palette_count > 256) palette_count = 256;
    }
    if ((unsigned int)off + (unsigned int)palette_count * 4 > size) return -1;
    const unsigned char *palette = dib + off;
    off += palette_count * 4;

    int xor_stride = ((w * bpp + 31) / 32) * 4;
    int and_stride = ((w + 31) / 32) * 4;
    if ((unsigned int)off + (unsigned int)xor_stride * h +
        (unsigned int)and_stride * h > size)
        return -1;

    const unsigned char *xor = dib + off;
    const unsigned char *and = xor + xor_stride * h;

    /* Some old 32bpp cursors leave the alpha byte zero and rely on the AND
     * mask. Detect that case once so those assets still render. */
    int use_alpha = 0;
    if (bpp == 32 && am) {
        for (int row = 0; row < h && !use_alpha; row++) {
            const unsigned char *src = xor + (h - 1 - row) * xor_stride;
            for (int x = 0; x < w; x++) {
                if (extract_channel(rd32(src + x * 4), am) != 0) {
                    use_alpha = 1;
                    break;
                }
            }
        }
    }

    for (int row = 0; row < h; row++) {
        const unsigned char *src = xor + (h - 1 - row) * xor_stride;
        const unsigned char *mask = and + (h - 1 - row) * and_stride;
        for (int x = 0; x < w; x++) {
            int r = 0, g = 0, b = 0, a = 255;
            int masked = (mask[x >> 3] & (0x80 >> (x & 7))) ? 1 : 0;

            if (bpp == 32) {
                unsigned int v = rd32(src + x * 4);
                r = extract_channel(v, rm);
                g = extract_channel(v, gm);
                b = extract_channel(v, bm);
                if (use_alpha) a = extract_channel(v, am);
            } else if (bpp == 24) {
                const unsigned char *p = src + x * 3;
                b = p[0]; g = p[1]; r = p[2];
            } else if (bpp == 16) {
                unsigned int v = rd16(src + x * 2);
                r = extract_channel(v, rm);
                g = extract_channel(v, gm);
                b = extract_channel(v, bm);
            } else {
                unsigned int idx = 0;
                if (bpp == 8) {
                    idx = src[x];
                } else if (bpp == 4) {
                    idx = (x & 1) ? (src[x >> 1] & 0x0F) : (src[x >> 1] >> 4);
                } else {
                    idx = (src[x >> 3] >> (7 - (x & 7))) & 1;
                }
                if ((int)idx >= palette_count) idx = 0;
                const unsigned char *p = palette + idx * 4;
                b = p[0]; g = p[1]; r = p[2];
            }

            /* Render the CUR's own pixels verbatim: its colors and its
             * alpha channel.  No inversion, no monochrome conversion. */
            int idx = row * w + x;
            out_color[idx] = gfx_rgb((unsigned char)r, (unsigned char)g,
                                     (unsigned char)b);
            if (bpp == 32 && use_alpha)
                out_alpha[idx] = (unsigned char)a;
            else
                out_alpha[idx] = (unsigned char)(masked ? 0 : 255);
        }
    }

    *out_w = w;
    *out_h = h;
    return 0;
}

static int decode_cur(const unsigned char *buf, unsigned int size,
                      int *out_w, int *out_h, int *out_hx, int *out_hy,
                      unsigned int *out_color, unsigned char *out_alpha) {
    if (size < 22 || rd16(buf) != 0) return -1;
    unsigned int type = rd16(buf + 2);
    if (type != 1 && type != 2) return -1;
    unsigned int count = rd16(buf + 4);
    if (!count || count > 64) return -1;

    int best = -1;
    int best_score = -1;
    int best_bpp = 0;

    for (unsigned int i = 0; i < count; i++) {
        const unsigned char *e = buf + 6 + 16 * i;
        unsigned int bytes = rd32(e + 8);
        unsigned int off = rd32(e + 12);
        if (off + 40 > size || off + bytes > size || bytes < 40) continue;

        int w = buf[off + 4] ? buf[off + 4] : 256;
        int h = (buf[off + 8] ? buf[off + 8] : 256) / 2;
        int bpp = rd16(buf + off + 14);
        int score = bpp * 100;
        if (w == X8_CURSOR_MAX_W && h == X8_CURSOR_MAX_H) score += 40;
        if (w <= X8_CURSOR_MAX_W && h <= X8_CURSOR_MAX_H) score += 20;
        if (score > best_score || (score == best_score && bpp > best_bpp)) {
            best_score = score;
            best_bpp = bpp;
            best = (int)i;
        }
    }
    if (best < 0) return -1;

    const unsigned char *e = buf + 6 + 16 * best;
    unsigned int bytes = rd32(e + 8);
    unsigned int off = rd32(e + 12);
    if (decode_dib(buf + off, bytes, out_w, out_h, out_color, out_alpha) != 0)
        return -1;

    *out_hx = rd16(e + 4);
    *out_hy = rd16(e + 6);
    if (*out_hx >= *out_w) *out_hx = *out_w - 1;
    if (*out_hy >= *out_h) *out_hy = *out_h - 1;
    return 0;
}

static int decode_ani(const unsigned char *buf, unsigned int size,
                      x8_cursor_t *cur) {
    if (size < 56 || rd32(buf + 8) != 0x4E4F4341u) /* 'ACON' */ return -1;
    unsigned int jif = size >= 52 ? rd32(buf + 48) : 2;
    cur->delay_ms = (int)((jif * 1000) / 60);
    if (cur->delay_ms < 16) cur->delay_ms = 33;

    unsigned int off = 12;
    while (off + 8 <= size) {
        unsigned int chunk = rd32(buf + off);
        unsigned int len = rd32(buf + off + 4);
        unsigned int next = off + 8 + len + (len & 1);
        if (next < off || off + 8 + len > size) break;

        if (chunk == 0x5453494Cu /* 'LIST' */ && len >= 4 &&
            rd32(buf + off + 8) == 0x6D617266u /* 'fram' */) {
            unsigned int sub = off + 12;
            unsigned int end = off + 8 + len;
            while (sub + 8 <= end && cur->count < X8_CURSOR_MAX_FRAMES) {
                unsigned int sub_id = rd32(buf + sub);
                unsigned int sub_len = rd32(buf + sub + 4);
                if (sub + 8 + sub_len > end) break;
                if (sub_id == 0x6E6F6369u /* 'icon' */) {
                    int w, h, hx, hy;
                    if (decode_cur(buf + sub + 8, sub_len, &w, &h, &hx, &hy,
                                   scratch_color, scratch_alpha) == 0) {
                        int f = cur->count;
                        cur->w = w;
                        cur->h = h;
                        cur->hot_x[f] = hx;
                        cur->hot_y[f] = hy;
                        mem_copy(cur->color[f], scratch_color,
                                 X8_CURSOR_PIXELS * sizeof(unsigned int));
                        mem_copy(cur->alpha[f], scratch_alpha,
                                 X8_CURSOR_PIXELS);
                        cur->count++;
                    }
                }
                sub += 8 + sub_len + (sub_len & 1);
            }
        }
        off = next;
    }
    return cur->count > 0 ? 0 : -1;
}

int x8_cursor_load(x8_cursor_t *cur, const char *path) {
    const void *data = 0;
    uint32_t size = 0;
    if (!cur || fs_read(path, &data, &size) != 0) return -1;
    if (size < 22) return -1;

    mem_set(cur, 0, sizeof(*cur));
    cur->delay_ms = 100;

    if (rd32((const unsigned char *)data) == 0x46464952u /* 'RIFF' */)
        return decode_ani((const unsigned char *)data, size, cur);

    int w, h, hx, hy;
    if (decode_cur((const unsigned char *)data, size, &w, &h, &hx, &hy,
                   cur->color[0], cur->alpha[0]) != 0)
        return -1;

    cur->w = w;
    cur->h = h;
    cur->hot_x[0] = hx;
    cur->hot_y[0] = hy;
    cur->count = 1;
    return 0;
}

void x8_cursor_reset(x8_cursor_t *cur) {
    if (cur) cur->frame = 0;
}

void x8_cursor_set_frame(x8_cursor_t *cur, int frame) {
    if (!cur || cur->count <= 0) return;
    if (frame < 0) frame = 0;
    if (frame >= cur->count) frame = cur->count - 1;
    cur->frame = frame;
}

void x8_cursor_animate(x8_cursor_t *cur, uint64_t now_ms) {
    if (!cur || cur->count <= 1 || cur->delay_ms <= 0) return;
    cur->frame = (int)((now_ms / (unsigned int)cur->delay_ms) %
                       (unsigned int)cur->count);
}

void x8_cursor_hide(void) {
    if (!saved_active) return;
    /* Restore only the pixels we actually overwrote.  The transparent parts
     * of the bounding box belong to the app, not to us. */
    int total = saved_w * saved_h;
    for (int k = 0; k < total; k++) {
        if (!saved_mask[k]) continue;
        gfx_pixel(saved_x + k % saved_w, saved_y + k / saved_w,
                  saved_pixels[k]);
    }
    saved_active = 0;
    saved_cur = 0;
}

void x8_cursor_draw(x8_cursor_t *cur, int x, int y) {
    if (!cur || cur->count <= 0) return;

    int frame = cur->frame;
    if (frame < 0 || frame >= cur->count) frame = 0;

    int same = saved_active && saved_cur == cur && saved_frame == frame &&
               saved_draw_x == x && saved_draw_y == y &&
               saved_hot_x == cur->hot_x[frame] &&
               saved_hot_y == cur->hot_y[frame];

    /* Don't churn pixels for a stationary pointer.  The pixel check also
     * catches an app that repainted the background behind us, which used to
     * leave a stale cursor image (or a trail) on screen. */
    if (same && cursor_pixels_intact()) return;

    if (saved_active) {
        if (same) {
            /* Background was repainted over the cursor; those saved pixels
             * are stale, so discard them instead of restoring them. */
            saved_active = 0;
        } else {
            x8_cursor_hide();
        }
    }

    int left = x - cur->hot_x[frame];
    int top = y - cur->hot_y[frame];
    int fbw = gfx_width(), fbh = gfx_height();

    int sx0 = left < 0 ? 0 : left;
    int sy0 = top < 0 ? 0 : top;
    int sx1 = left + cur->w > fbw ? fbw : left + cur->w;
    int sy1 = top + cur->h > fbh ? fbh : top + cur->h;
    if (sx1 <= sx0 || sy1 <= sy0) return;

    blend_init();
    saved_x = sx0;
    saved_y = sy0;
    saved_w = sx1 - sx0;
    saved_h = sy1 - sy0;

    for (int row = 0; row < saved_h; row++) {
        for (int col = 0; col < saved_w; col++) {
            int px = saved_x + col, py = saved_y + row;
            int k = row * saved_w + col;
            int a = 0;
            int src = 0;

            int li = (py - top) * cur->w + (px - left);
            if (li >= 0 && li < cur->w * cur->h) {
                int fi = frame * X8_CURSOR_PIXELS + li;
                int ci = fi / X8_CURSOR_PIXELS;
                int pi = fi % X8_CURSOR_PIXELS;
                a = cur->alpha[ci][pi];
                src = cur->color[ci][pi];
            }

            /* Skip fully transparent pixels entirely -- no read, no write,
             * no restore later.  Only visible pixels can cause jitter. */
            saved_mask[k] = (unsigned char)(a > 0 ? 1 : 0);
            if (!a) continue;

            unsigned int bg = gfx_getpixel(px, py);
            saved_pixels[k] = bg;
            gfx_pixel(px, py, blend_pixel(bg, src, a));
            drawn_pixels[k] = gfx_getpixel(px, py);
        }
    }
    saved_cur = cur;
    saved_frame = frame;
    saved_draw_x = x;
    saved_draw_y = y;
    saved_hot_x = cur->hot_x[frame];
    saved_hot_y = cur->hot_y[frame];
    saved_active = 1;
}

int x8_cursor_hot_x(const x8_cursor_t *cur) {
    return cur && cur->count ? cur->hot_x[cur->frame] : 0;
}

int x8_cursor_hot_y(const x8_cursor_t *cur) {
    return cur && cur->count ? cur->hot_y[cur->frame] : 0;
}

int x8_cursor_frames(const x8_cursor_t *cur) {
    return cur ? cur->count : 0;
}
