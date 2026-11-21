#include "vfs.h"
#include "initramfs.h"
#include "serial.h"

#define VFS_MAX_FILES   64
#define VFS_NAME_POOL   4096
#define VFS_DATA_POOL   (4 * 1024 * 1024)
#define VFS_NAME_LEN    64
#define VFS_MAX_LIST    64

typedef struct {
    char *name;
    unsigned char *data;
    uint32_t size;
    int used;
} vfs_entry_t;

static vfs_entry_t entries[VFS_MAX_FILES];
static char name_pool[VFS_NAME_POOL];
static unsigned char data_pool[VFS_DATA_POOL];
static uint32_t name_brk;
static uint32_t data_brk;

static uint32_t strlen_b(const char *s) {
    uint32_t n = 0;
    while (*s++) n++;
    return n;
}

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void norm(const char *path, char *out, uint32_t max) {
    while (*path == '/') path++;
    uint32_t n = strlen_b(path);
    while (n > 0 && path[n - 1] == '/') n--;
    if (n >= max) n = max - 1;
    for (uint32_t i = 0; i < n; i++) out[i] = path[i];
    out[n] = '\0';
}

void vfs_init(void) {
    for (int i = 0; i < VFS_MAX_FILES; i++) entries[i].used = 0;
    name_brk = 0;
    data_brk = 0;

    uint32_t count = initramfs_count();
    if (count > VFS_MAX_FILES) count = VFS_MAX_FILES;

    for (uint32_t i = 0; i < count; i++) {
        initramfs_file_t *f = initramfs_get(i);
        if (!f) continue;

        uint32_t nlen = f->name_len;
        if (nlen >= VFS_NAME_LEN || name_brk + nlen + 1 > VFS_NAME_POOL) continue;
        if (data_brk + f->data_len > VFS_DATA_POOL) continue;

        char *name = name_pool + name_brk;
        unsigned char *data = data_pool + data_brk;
        for (uint32_t j = 0; j < nlen; j++) name[j] = f->name[j];
        name[nlen] = '\0';
        for (uint32_t j = 0; j < f->data_len; j++) data[j] = f->data[j];

        entries[i].name = name;
        entries[i].data = data;
        entries[i].size = f->data_len;
        entries[i].used = 1;

        name_brk += nlen + 1;
        data_brk += f->data_len;
    }

    serial_puts("[vfs] loaded ");
    serial_puthex(count);
    serial_puts(" files\n");
}

static vfs_entry_t *vfs_find(const char *path) {
    char name[VFS_NAME_LEN];
    norm(path, name, sizeof(name));
    if (!name[0]) return 0;
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (entries[i].used && streq(entries[i].name, name)) return &entries[i];
    }
    return 0;
}

int vfs_read(const char *path, const void **data, uint32_t *size) {
    vfs_entry_t *e = vfs_find(path);
    if (!e) return -1;
    *data = e->data;
    *size = e->size;
    return 0;
}

int vfs_write(const char *path, const void *data, uint32_t size) {
    char name[VFS_NAME_LEN];
    norm(path, name, sizeof(name));
    if (!name[0]) return -1;

    vfs_entry_t *e = vfs_find(name);
    if (e) {
        if (size > VFS_DATA_POOL) return -1;
        for (uint32_t i = 0; i < size; i++) e->data[i] = ((const unsigned char *)data)[i];
        e->size = size;
        return 0;
    }

    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (entries[i].used) continue;
        uint32_t nlen = strlen_b(name);
        if (name_brk + nlen + 1 > VFS_NAME_POOL) return -1;
        if (data_brk + size > VFS_DATA_POOL) return -1;

        char *dst_name = name_pool + name_brk;
        unsigned char *dst_data = data_pool + data_brk;
        for (uint32_t j = 0; j < nlen; j++) dst_name[j] = name[j];
        dst_name[nlen] = '\0';
        for (uint32_t j = 0; j < size; j++) dst_data[j] = ((const unsigned char *)data)[j];

        entries[i].name = dst_name;
        entries[i].data = dst_data;
        entries[i].size = size;
        entries[i].used = 1;
        name_brk += nlen + 1;
        data_brk += size;
        return 0;
    }
    return -1;
}

int vfs_remove(const char *path) {
    vfs_entry_t *e = vfs_find(path);
    if (!e) return -1;
    e->used = 0;
    return 0;
}

static void add_unique(char list[VFS_MAX_LIST][VFS_NAME_LEN], int *count, const char *name) {
    for (int i = 0; i < *count; i++) {
        if (streq(list[i], name)) return;
    }
    if (*count >= VFS_MAX_LIST) return;
    uint32_t n = strlen_b(name);
    if (n >= VFS_NAME_LEN) n = VFS_NAME_LEN - 1;
    for (uint32_t i = 0; i < n; i++) list[(*count)][i] = name[i];
    list[(*count)][n] = '\0';
    (*count)++;
}

/* A directory exists only if some file lives below it. */
int vfs_is_dir(const char *path) {
    char base[VFS_NAME_LEN];
    norm(path, base, sizeof(base));
    if (!base[0]) return 1;                 /* root always exists */

    uint32_t blen = strlen_b(base);
    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!entries[i].used) continue;
        const char *name = entries[i].name;
        if (strlen_b(name) <= blen) continue;
        uint32_t j = 0;
        for (; j < blen; j++) {
            if (base[j] != name[j]) break;
        }
        if (j == blen && name[blen] == '/') return 1;
    }
    return 0;
}

int vfs_list(const char *dir, char *buf, uint32_t bufsize) {
    char base[VFS_NAME_LEN];
    norm(dir, base, sizeof(base));

    if (base[0] && !vfs_is_dir(base)) return -1;

    static char list[VFS_MAX_LIST][VFS_NAME_LEN];
    int count = 0;

    for (int i = 0; i < VFS_MAX_FILES; i++) {
        if (!entries[i].used) continue;
        const char *name = entries[i].name;

        if (base[0]) {
            uint32_t blen = strlen_b(base);
            uint32_t nlen = strlen_b(name);
            if (nlen <= blen || name[blen] != '/') continue;
            uint32_t j = 0;
            for (; j < blen; j++) {
                if (base[j] != name[j]) break;
            }
            if (j != blen) continue;
            name += blen + 1;
        }

        char child[VFS_NAME_LEN];
        uint32_t n = 0;
        while (name[n] && name[n] != '/' && n < VFS_NAME_LEN - 2) {
            child[n] = name[n];
            n++;
        }
        if (name[n] == '/') child[n++] = '/';
        child[n] = '\0';
        if (n) add_unique(list, &count, child);
    }

    uint32_t off = 0;
    for (int i = 0; i < count; i++) {
        uint32_t n = strlen_b(list[i]);
        if (off + n + 2 > bufsize) break;
        for (uint32_t j = 0; j < n; j++) buf[off++] = list[i][j];
        buf[off++] = '\n';
    }
    buf[off] = '\0';
    return count;
}
