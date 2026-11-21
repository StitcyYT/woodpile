#pragma once

#include <stdint.h>

typedef struct {
    const char *name;
    uint32_t name_len;
    uint32_t data_len;
    const uint8_t *data;
} initramfs_file_t;

void initramfs_init(void);
uint32_t initramfs_count(void);
initramfs_file_t *initramfs_get(uint32_t index);
initramfs_file_t *initramfs_find(const char *name);
