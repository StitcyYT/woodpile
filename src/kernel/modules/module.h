#ifndef MODULE_H
#define MODULE_H

#include <stdint.h>

/* Load an ELF64 relocatable ".co" image into memory and return its
 * module_entry address in *entry. */
int module_load(const uint8_t *data, uint32_t len, uint64_t *entry);

/* Find a .co file in the initramfs, load it and call module_entry(). */
int module_run_path(const char *path);

/* Filesystem wrapper used by modules. */
int fs_read(const char *path, const void **data, uint32_t *size);

#endif
