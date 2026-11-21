#include "module.h"
#include "initramfs.h"
#include "serial.h"
#include "framebuffer.h"
#include "time.h"
#include "mouse.h"

/* Everything a .co module is allowed to call. Kept deliberately small. */
typedef struct {
    const char *name;
    uint64_t addr;
} ksym_t;

extern void fb_clear(unsigned int color);
extern void fb_putpixel(int x, int y, unsigned int color);
extern unsigned int fb_getpixel(int x, int y);
extern void fb_putchar(int x, int y, char c, unsigned int fg, unsigned int bg);
extern void fb_drawstr(int x, int y, const char *s, unsigned int fg, unsigned int bg);
extern unsigned int fb_color(unsigned char r, unsigned char g, unsigned char b);
extern int fb_width(void);
extern int fb_height(void);
extern void mouse_get(int *x, int *y);
extern void serial_puts(const char *s);
extern void serial_puthex(uint64_t v);
extern void time_sleep(int ms);

static const ksym_t ksyms[] = {
    { "fb_clear",   (uint64_t)&fb_clear   },
    { "fb_putpixel",(uint64_t)&fb_putpixel},
    { "fb_getpixel",(uint64_t)&fb_getpixel},
    { "fb_putchar", (uint64_t)&fb_putchar },
    { "fb_drawstr", (uint64_t)&fb_drawstr },
    { "fb_color",   (uint64_t)&fb_color   },
    { "fb_width",   (uint64_t)&fb_width   },
    { "fb_height",  (uint64_t)&fb_height  },
    { "mouse_get",  (uint64_t)&mouse_get  },
    { "fs_read",    (uint64_t)&fs_read    },
    { "module_run_path", (uint64_t)&module_run_path },
    { "serial_puts",(uint64_t)&serial_puts},
    { "serial_puthex",(uint64_t)&serial_puthex },
    { "time_sleep", (uint64_t)&time_sleep },
};

#define MAX_SECTIONS 64

/* Modules are placed in identity-mapped RAM above the initramfs/stack. */
static uint64_t mod_brk = 0x100000;

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t rd64(const uint8_t *p) {
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}
static void wr32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}
static void wr64(uint8_t *p, uint64_t v) {
    wr32(p, (uint32_t)v); wr32(p + 4, (uint32_t)(v >> 32));
}
static void memzero(uint8_t *p, uint64_t n) {
    while (n--) *p++ = 0;
}
static void memcpy_b(uint8_t *d, const uint8_t *s, uint64_t n) {
    while (n--) *d++ = *s++;
}
static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* ELF64 section header byte offsets. */
#define SH_TYPE(s)   rd32((s) + 4)
#define SH_FLAGS(s)  rd64((s) + 8)
#define SH_OFFSET(s) rd64((s) + 24)
#define SH_SIZE(s)   rd64((s) + 32)
#define SH_LINK(s)   rd32((s) + 40)
#define SH_INFO(s)   rd32((s) + 44)
#define SH_ALIGN(s)  rd64((s) + 48)

#define SHT_SYMTAB 2
#define SHT_RELA   4
#define SHT_NOBITS 8
#define SHF_ALLOC  2
#define SHN_UNDEF  0
#define SHN_ABS    0xFFF1

#define SYM_NAME(s)  rd32((s))
#define SYM_SHNDX(s) rd16((s) + 6)
#define SYM_VALUE(s) rd64((s) + 8)

#define RELA_OFF(r)  rd64((r))
#define RELA_INFO(r) rd64((r) + 8)
#define RELA_ADD(r)  ((int64_t)rd64((r) + 16))

static uint64_t resolve_symbol(uint32_t idx, const uint8_t *symtab,
                               const uint8_t *strtab, const uint8_t *sh,
                               const uint64_t *sec_addr, uint16_t shnum) {
    const uint8_t *sym = symtab + idx * 24;
    uint16_t shndx = SYM_SHNDX(sym);
    uint64_t value = SYM_VALUE(sym);

    if (shndx == SHN_UNDEF) {
        const char *name = (const char *)(strtab + SYM_NAME(sym));
        for (uint32_t i = 0; i < sizeof(ksyms) / sizeof(ksyms[0]); i++)
            if (streq(name, ksyms[i].name)) return ksyms[i].addr;
        serial_puts("[mod] unresolved symbol: ");
        serial_puts(name);
        serial_puts("\n");
        return 0;
    }
    if (shndx == SHN_ABS) return value;
    if (shndx < shnum && sec_addr[shndx]) return sec_addr[shndx] + value;
    return value;
}

int module_load(const uint8_t *data, uint32_t len, uint64_t *entry) {
    if (!data || len < 64 || rd32(data) != 0x464C457F) return -1;
    if (data[4] != 2 || data[5] != 1) return -1;        /* ELF64, little endian */
    if (rd16(data + 16) != 1) return -1;                /* ET_REL */
    if (rd16(data + 18) != 62) return -1;               /* x86-64 */

    uint64_t shoff = rd64(data + 40);
    uint16_t shentsize = rd16(data + 58);
    uint16_t shnum = rd16(data + 60);
    if (shentsize != 64 || shnum == 0 || shnum > MAX_SECTIONS) return -2;

    const uint8_t *sh = data + shoff;
    uint64_t sec_addr[MAX_SECTIONS];
    for (uint16_t i = 0; i < MAX_SECTIONS; i++) sec_addr[i] = 0;

    /* Pass 1: assign an address to every allocated section. */
    for (uint16_t i = 0; i < shnum; i++) {
        const uint8_t *s = sh + i * 64;
        uint64_t size = SH_SIZE(s);
        if (!(SH_FLAGS(s) & SHF_ALLOC) || size == 0) continue;
        uint64_t align = SH_ALIGN(s);
        if (align < 1) align = 1;
        mod_brk = (mod_brk + align - 1) & ~(align - 1);
        sec_addr[i] = mod_brk;
        mod_brk += size;
    }

    /* Pass 2: load section contents. */
    for (uint16_t i = 0; i < shnum; i++) {
        if (!sec_addr[i]) continue;
        const uint8_t *s = sh + i * 64;
        uint64_t size = SH_SIZE(s);
        if (SH_TYPE(s) == SHT_NOBITS) memzero((uint8_t *)sec_addr[i], size);
        else memcpy_b((uint8_t *)sec_addr[i], data + SH_OFFSET(s), size);
    }

    /* Pass 3: apply relocations. */
    for (uint16_t i = 0; i < shnum; i++) {
        const uint8_t *s = sh + i * 64;
        if (SH_TYPE(s) != SHT_RELA) continue;
        uint32_t target = SH_INFO(s);
        if (target >= shnum || !sec_addr[target]) continue;

        const uint8_t *symsec = sh + SH_LINK(s) * 64;
        const uint8_t *symtab = data + SH_OFFSET(symsec);
        const uint8_t *strsec = sh + SH_LINK(symsec) * 64;
        const uint8_t *strtab = data + SH_OFFSET(strsec);
        uint64_t n = SH_SIZE(s) / 24;
        const uint8_t *r = data + SH_OFFSET(s);

        for (uint64_t k = 0; k < n; k++) {
            const uint8_t *rela = r + k * 24;
            uint64_t info = RELA_INFO(rela);
            uint32_t symidx = (uint32_t)(info >> 32);
            uint32_t type = (uint32_t)(info & 0xFFFFFFFF);
            uint64_t S = resolve_symbol(symidx, symtab, strtab, sh, sec_addr, shnum);
            uint8_t *P = (uint8_t *)(sec_addr[target] + RELA_OFF(rela));
            int64_t A = RELA_ADD(rela);

            switch (type) {
                case 1:  wr64(P, S + (uint64_t)A); break;                 /* 64   */
                case 2:                                                   /* PC32 */
                case 4:  wr32(P, (uint32_t)(S + (uint64_t)A - (uint64_t)P)); break; /* PLT32 */
                case 10: wr32(P, (uint32_t)(S + (uint64_t)A)); break;     /* 32   */
                case 11: wr32(P, (uint32_t)(S + (uint64_t)A)); break;     /* 32S  */
                default:
                    serial_puts("[mod] reloc type ");
                    serial_puthex(type);
                    serial_puts(" skipped\n");
                    break;
            }
        }
    }

    /* Pass 4: locate module_entry. */
    for (uint16_t i = 0; i < shnum; i++) {
        const uint8_t *s = sh + i * 64;
        if (SH_TYPE(s) != SHT_SYMTAB) continue;
        const uint8_t *symtab = data + SH_OFFSET(s);
        const uint8_t *strtab = data + SH_OFFSET(sh + SH_LINK(s) * 64);
        uint64_t n = SH_SIZE(s) / 24;
        for (uint64_t k = 0; k < n; k++) {
            const uint8_t *sym = symtab + k * 24;
            const char *name = (const char *)(strtab + SYM_NAME(sym));
            if (!streq(name, "module_entry")) continue;
            uint16_t shndx = SYM_SHNDX(sym);
            if (shndx < shnum && sec_addr[shndx]) {
                *entry = sec_addr[shndx] + SYM_VALUE(sym);
                return 0;
            }
        }
    }
    return -3;
}

/* /devc/umouse is a live 8-byte pseudo-file: uint32 x, uint32 y. */
static uint32_t umouse_buf[2];

static int path_eq(const char *a, const char *b) {
    while (*a == '/') a++;
    while (*b == '/') b++;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

int fs_read(const char *path, const void **data, uint32_t *size) {
    if (path && path_eq(path, "devc/umouse")) {
        int x, y;
        mouse_get(&x, &y);
        umouse_buf[0] = (uint32_t)x;
        umouse_buf[1] = (uint32_t)y;
        *data = umouse_buf;
        *size = 8;
        return 0;
    }

    initramfs_file_t *f = initramfs_find(path);
    if (!f) return -1;
    *data = f->data;
    *size = f->data_len;
    return 0;
}

int module_run_path(const char *path) {
    initramfs_file_t *f = initramfs_find(path);
    if (!f) {
        serial_puts("[mod] missing file: ");
        serial_puts(path);
        serial_puts("\n");
        return -1;
    }
    uint64_t entry = 0;
    int rc = module_load(f->data, f->data_len, &entry);
    if (rc != 0) {
        serial_puts("[mod] load failed ");
        serial_puthex((uint64_t)rc);
        serial_puts("\n");
        return rc;
    }
    serial_puts("[mod] running ");
    serial_puts(path);
    serial_puts("\n");
    ((void (*)(void))entry)();
    return 0;
}
