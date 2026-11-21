#ifndef VFS_H
#define VFS_H

#include <stdint.h>

void vfs_init(void);

int vfs_read(const char *path, const void **data, uint32_t *size);
int vfs_write(const char *path, const void *data, uint32_t size);
int vfs_remove(const char *path);
int vfs_is_dir(const char *path);

/* Writes newline-separated child names into buf; directories end in '/'.
 * Returns the number of entries. */
int vfs_list(const char *dir, char *buf, uint32_t bufsize);

#endif
