#include "inflate.h"
#include "xecabi.h"

typedef struct {
    const uint8_t *src;
    uint32_t len;
    uint32_t pos;
    uint32_t bits;
    int count;
} bitstream_t;

typedef struct {
    uint16_t count[16];
    uint16_t symbol[288];
} huff_t;

static int get_bits(bitstream_t *b, int n) {
    int value = 0;
    for (int i = 0; i < n; i++) {
        if (!b->count) {
            if (b->pos >= b->len) return -1;
            b->bits = b->src[b->pos++];
            b->count = 8;
        }
        value |= (int)(b->bits & 1) << i;
        b->bits >>= 1;
        b->count--;
    }
    return value;
}

static void huff_clear(huff_t *h) {
    for (int i = 0; i < 16; i++) h->count[i] = 0;
}

static int huff_build(huff_t *h, const uint8_t *lengths, int n) {
    huff_clear(h);
    for (int i = 0; i < n; i++) {
        int len = lengths[i];
        if (len < 0 || len > 15) return -1;
        if (len) h->count[len]++;
    }

    uint16_t next[16];
    int code = 0;
    for (int len = 1; len <= 15; len++) {
        code = (code + (len == 1 ? 0 : h->count[len - 1])) << 1;
        next[len] = (uint16_t)code;
    }
    for (int i = 0; i < n; i++) {
        int len = lengths[i];
        if (len) h->symbol[next[len]++] = (uint16_t)i;
    }
    return 0;
}

static int huff_decode(bitstream_t *b, const huff_t *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; len++) {
        int bit = get_bits(b, 1);
        if (bit < 0) return -1;
        code |= bit;
        int count = h->count[len];
        if (code - first < count) return h->symbol[index + (code - first)];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

static const uint16_t len_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const uint8_t len_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const uint16_t dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,
    2049,3073,4097,6145,8193,12289,16385,24577
};
static const uint8_t dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

static const uint8_t clen_order[19] = {
    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
};

int wz_inflate(const void *src, uint32_t src_len, void *dst, uint32_t dst_cap) {
    const uint8_t *s = (const uint8_t *)src;
    uint8_t *out = (uint8_t *)dst;
    if (!s || !out || src_len < 2) return -1;
    if ((s[0] & 0x0F) != 8) return -1;
    if ((((unsigned)s[0] << 8) | s[1]) % 31) return -1;

    bitstream_t b = { s + 2, src_len - 2, 0, 0, 0 };
    uint32_t written = 0;

    for (;;) {
        int final = get_bits(&b, 1);
        int type = get_bits(&b, 2);
        if (final < 0 || type < 0) return -1;

        if (type == 0) {
            b.count = 0;
            int len = get_bits(&b, 16);
            int nlen = get_bits(&b, 16);
            if (len < 0 || nlen < 0 || ((len ^ 0xFFFF) & 0xFFFF) != nlen) return -1;
            if (written + (uint32_t)len > dst_cap) return -1;
            if (b.pos + (uint32_t)len > b.len) return -1;
            mem_copy(out + written, b.src + b.pos, (unsigned int)len);
            b.pos += (uint32_t)len;
            written += (uint32_t)len;
        } else if (type == 1 || type == 2) {
            huff_t lit, dist;
            huff_clear(&lit);
            huff_clear(&dist);

            if (type == 1) {
                uint8_t lengths[288];
                for (int i = 0; i < 288; i++) lengths[i] = (uint8_t)(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
                if (huff_build(&lit, lengths, 288)) return -1;
                for (int i = 0; i < 30; i++) lengths[i] = 5;
                if (huff_build(&dist, lengths, 30)) return -1;
            } else {
                int hlit = get_bits(&b, 5);
                int hdist = get_bits(&b, 5);
                int hclen = get_bits(&b, 4);
                if (hlit < 0 || hdist < 0 || hclen < 0) return -1;
                hlit += 257;
                hdist += 1;
                hclen += 4;
                if (hlit > 286 || hdist > 30) return -1;

                uint8_t clengths[19];
                mem_set(clengths, 0, sizeof(clengths));
                for (int i = 0; i < hclen; i++) {
                    int v = get_bits(&b, 3);
                    if (v < 0) return -1;
                    clengths[clen_order[i]] = (uint8_t)v;
                }
                huff_t clen;
                if (huff_build(&clen, clengths, 19)) return -1;

                uint8_t lengths[320];
                int total = hlit + hdist;
                int i = 0;
                while (i < total) {
                    int sym = huff_decode(&b, &clen);
                    if (sym < 0) return -1;
                    if (sym < 16) {
                        lengths[i++] = (uint8_t)sym;
                    } else if (sym == 16) {
                        if (i == 0) return -1;
                        int rep = get_bits(&b, 2);
                        if (rep < 0) return -1;
                        uint8_t prev = lengths[i - 1];
                        for (int j = 0; j < rep + 3 && i < total; j++) lengths[i++] = prev;
                    } else if (sym == 17) {
                        int rep = get_bits(&b, 3);
                        if (rep < 0) return -1;
                        for (int j = 0; j < rep + 3 && i < total; j++) lengths[i++] = 0;
                    } else {
                        int rep = get_bits(&b, 7);
                        if (rep < 0) return -1;
                        for (int j = 0; j < rep + 11 && i < total; j++) lengths[i++] = 0;
                    }
                }
                if (huff_build(&lit, lengths, hlit)) return -1;
                if (huff_build(&dist, lengths + hlit, hdist)) return -1;
            }

            for (;;) {
                int sym = huff_decode(&b, &lit);
                if (sym < 0 || sym > 285) return -1;
                if (sym < 256) {
                    if (written >= dst_cap) return -1;
                    out[written++] = (uint8_t)sym;
                    continue;
                }
                if (sym == 256) break;

                int li = sym - 257;
                int length = len_base[li];
                int extra = get_bits(&b, len_extra[li]);
                if (extra < 0) return -1;
                length += extra;

                int dsym = huff_decode(&b, &dist);
                if (dsym < 0 || dsym >= 30) return -1;
                int distance = dist_base[dsym];
                extra = get_bits(&b, dist_extra[dsym]);
                if (extra < 0) return -1;
                distance += extra;

                if ((uint32_t)distance > written) return -1;
                if (written + (uint32_t)length > dst_cap) return -1;
                uint8_t *p = out + written - distance;
                for (int j = 0; j < length; j++) out[written++] = *p++;
            }
        } else {
            return -1;
        }

        if (final) break;
    }
    return (int)written;
}
