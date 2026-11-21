#ifndef INFLATE_H
#define INFLATE_H

#include <stdint.h>

/* Minimal zlib/DEFLATE decoder. Returns decompressed byte count, or -1. */
int wz_inflate(const void *src, uint32_t src_len, void *dst, uint32_t dst_cap);

#endif
