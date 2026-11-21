#include "xecabi.h"

unsigned int str_len(const char *s) {
    unsigned int n = 0;
    while (*s++) n++;
    return n;
}

int str_cmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int str_eq(const char *a, const char *b) {
    return str_cmp(a, b) == 0;
}

const char *str_str(const char *haystack, const char *needle) {
    if (!*needle) return haystack;
    for (const char *p = haystack; *p; p++) {
        const char *a = p, *b = needle;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*b) return p;
    }
    return 0;
}

void str_copy(char *dst, const char *src) {
    while ((*dst++ = *src++) != '\0') { }
}

void mem_copy(void *dst, const void *src, unsigned int n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (unsigned int i = 0; i < n; i++) d[i] = s[i];
}

void mem_set(void *dst, int c, unsigned int n) {
    unsigned char *d = dst;
    for (unsigned int i = 0; i < n; i++) d[i] = (unsigned char)c;
}

/* GCC may still emit these for loop idioms even with -fno-builtin. */
void *memcpy(void *dst, const void *src, unsigned long n) {
    mem_copy(dst, src, (unsigned int)n);
    return dst;
}
void *memset(void *dst, int c, unsigned long n) {
    mem_set(dst, c, (unsigned int)n);
    return dst;
}

void num_to_str(unsigned long v, char *out) {
    char tmp[24];
    int n = 0;
    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while (v) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = '\0';
}
