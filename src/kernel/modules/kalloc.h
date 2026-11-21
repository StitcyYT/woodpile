#ifndef KALLOC_H
#define KALLOC_H

#include <stddef.h>

void kalloc_init(void);
void *kalloc(size_t size);
void kfree(void *ptr);
void *krealloc(void *ptr, size_t size);

#endif
