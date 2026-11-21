#include "xec.h"
#include "vfs.h"
#include "serial.h"
#include "kbd.h"

#define XEC_MAGIC 0x31434558UL  /* "XEC1" */

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t entry;
    uint32_t code_size;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t load_addr;
} xec_header_t;

static void copy_bytes(unsigned char *dst, const unsigned char *src, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) dst[i] = src[i];
}
static void zero_bytes(unsigned char *dst, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) dst[i] = 0;
}

int xec_run(const char *path, const xec_args_t *args) {
    const void *raw = 0;
    uint32_t size = 0;

    if (vfs_read(path, &raw, &size) != 0) {
        serial_puts("[xec] not found: ");
        serial_puts(path);
        serial_puts("\n");
        return -1;
    }
    if (size < sizeof(xec_header_t)) {
        serial_puts("[xec] too small: ");
        serial_puts(path);
        serial_puts("\n");
        return -2;
    }

    const xec_header_t *h = (const xec_header_t *)raw;
    if (h->magic != XEC_MAGIC || h->version != 1) {
        serial_puts("[xec] bad header: ");
        serial_puts(path);
        serial_puts("\n");
        return -3;
    }

    uint32_t image_size = h->code_size + h->data_size;
    if (image_size + sizeof(xec_header_t) > size || h->load_addr == 0) {
        serial_puts("[xec] bad image: ");
        serial_puts(path);
        serial_puts("\n");
        return -4;
    }

    unsigned char *dst = (unsigned char *)(unsigned long)h->load_addr;
    copy_bytes(dst, (const unsigned char *)raw + sizeof(xec_header_t), image_size);
    zero_bytes(dst + image_size, h->bss_size);

    int (*entry)(const xec_args_t *) =
        (int (*)(const xec_args_t *))(dst + h->entry);

    serial_puts("[xec] run ");
    serial_puts(path);
    serial_puts("\n");

    /* The shell typed this command using the same raw queue GUI apps read. */
    kbd_flush();

    return entry(args);
}
