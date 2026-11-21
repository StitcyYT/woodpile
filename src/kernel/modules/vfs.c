#include "vfs.h"
#include "initramfs.h"
#include "serial.h"
#include "panic.h"

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

/* ------------------------------------------------------------------ */
/* Virtual procfs.  The PID space is far too large to materialise, so
 * /sys32/proc and /sys32/proc/<pid> are synthesised on demand. */
#define PROC_ROOT       "sys32/proc"
#define PROC_ROOT_LEN   (sizeof(PROC_ROOT) - 1)
#define PROC_MAX_PID    643011010u

/* proc_parse() returns a leaf pointer into this buffer, so it must outlive
 * the call.  A stack buffer here silently corrupts the leaf on return. */
static char proc_path_name[VFS_NAME_LEN];

static int proc_is_root(const char *path) {
    char name[VFS_NAME_LEN];
    norm(path, name, sizeof(name));
    return streq(name, PROC_ROOT);
}

static int proc_parse(const char *path, uint32_t *pid, const char **leaf) {
    char *name = proc_path_name;
    norm(path, name, VFS_NAME_LEN);

    const char prefix[] = PROC_ROOT "/";
    for (unsigned int i = 0; i < PROC_ROOT_LEN; i++) {
        if (name[i] != prefix[i]) return 0;
    }
    if (name[PROC_ROOT_LEN] != '/') return 0;

    const char *p = name + PROC_ROOT_LEN + 1;
    if (!*p) return 0;

    uint32_t value = 0;
    unsigned int digits = 0;
    while (*p >= '0' && *p <= '9') {
        value = value * 10u + (uint32_t)(*p - '0');
        digits++;
        p++;
    }
    if (!digits || value > PROC_MAX_PID || value == 0) return 0;
    if (*p == '/') p++;
    else if (*p) return 0;

    *pid = value;
    *leaf = p;
    return 1;
}

static unsigned int proc_append(char *buf, unsigned int *at, uint32_t cap,
                                const char *s) {
    while (*s && *at + 1 < cap) buf[(*at)++] = *s++;
    buf[*at] = 0;
    return *at;
}

static unsigned int proc_uint(char *buf, unsigned int *at, uint32_t cap,
                              uint32_t v) {
    char tmp[12];
    unsigned int n = 0;
    if (!v) tmp[n++] = '0';
    while (v && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (n && *at + 1 < cap) buf[(*at)++] = tmp[--n];
    buf[*at] = 0;
    return *at;
}

static int proc_read(uint32_t pid, const char *leaf,
                     const void **data, uint32_t *size) {
    static char buf[256];
    unsigned int n = 0;

    if (streq(leaf, "uinfo")) {
        proc_append(buf, &n, sizeof(buf), "pid=");
        proc_uint(buf, &n, sizeof(buf), pid);
        proc_append(buf, &n, sizeof(buf), "\nname=");
        proc_append(buf, &n, sizeof(buf), pid == 1 ? "kernel" : "process");
        proc_append(buf, &n, sizeof(buf), "\nstate=running\n");
    } else if (streq(leaf, "current")) {
        proc_append(buf, &n, sizeof(buf), "current=" PROC_ROOT "/");
        proc_uint(buf, &n, sizeof(buf), pid);
        proc_append(buf, &n, sizeof(buf), "\n");
    } else if (pid == 1 && streq(leaf, "nuserhandler")) {
        proc_append(buf, &n, sizeof(buf), "users/nuser.co\n");
    } else if (pid == 1 && streq(leaf, "sysrq")) {
        proc_append(buf, &n, sizeof(buf), "# write rcrq to trigger wallpaper panic\n");
    } else {
        return -1;
    }

    *data = buf;
    *size = n;
    return 0;
}

static int proc_write(const char *path, const void *data, uint32_t size) {
    char name[VFS_NAME_LEN];
    norm(path, name, sizeof(name));
    if (!streq(name, PROC_ROOT "/1/sysrq")) return -1;

    const unsigned char *b = (const unsigned char *)data;
    if (!b || size < 4) return -1;
    if (b[0] == 'r' && b[1] == 'c' && b[2] == 'r' && b[3] == 'q') {
        panic_worst("sysrq rcrq", 0, 0x000000000FFFFFFFULL);
        return 0;               /* never reached */
    }
    return -1;
}

static int proc_read_path(const char *path,
                          const void **data, uint32_t *size) {
    uint32_t pid;
    const char *leaf;
    if (!proc_parse(path, &pid, &leaf)) return -1;
    return proc_read(pid, leaf, data, size);
}

static int proc_is_dir(const char *path) {
    if (proc_is_root(path)) return 1;
    uint32_t pid;
    const char *leaf;
    return proc_parse(path, &pid, &leaf) && !leaf[0];
}

static int proc_list(const char *path, char *buf, uint32_t bufsize) {
    unsigned int n = 0;
    if (proc_is_root(path)) {
        proc_append(buf, &n, bufsize, "1/\n2/\n3/\n...\n643011010/\n");
        return 5;
    }

    uint32_t pid;
    const char *leaf;
    if (!proc_parse(path, &pid, &leaf) || leaf[0]) return -1;

    proc_append(buf, &n, bufsize, "uinfo\ncurrent\n");
    int count = 2;
    if (pid == 1) {
        proc_append(buf, &n, bufsize, "sysrq\nnuserhandler\n");
        count += 2;
    }
    return count;
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
    if (proc_read_path(path, data, size) == 0) return 0;

    vfs_entry_t *e = vfs_find(path);
    if (!e) return -1;
    *data = e->data;
    *size = e->size;
    return 0;
}

int vfs_write(const char *path, const void *data, uint32_t size) {
    if (proc_write(path, data, size) == 0) return 0;

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
    if (proc_is_dir(path)) return 1;

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

    int proc_count = proc_list(base, buf, bufsize);
    if (proc_count >= 0) return proc_count;

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
