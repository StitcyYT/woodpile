#include "ata.h"

#define ATA_DATA   0x1F0
#define ATA_SECNT  0x1F2
#define ATA_LBA0   0x1F3
#define ATA_LBA1   0x1F4
#define ATA_LBA2   0x1F5
#define ATA_DRIVE  0x1F6
#define ATA_STATUS 0x1F7
#define ATA_CMD    0x1F7

static inline unsigned char inb(unsigned short port) {
    unsigned char v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline unsigned short inw(unsigned short port) {
    unsigned short v;
    __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static int wait_ready(void) {
    for (unsigned int i = 0; i < 20000000; i++) {
        unsigned char st = inb(ATA_STATUS);
        if (!(st & 0x80)) {
            if (st & 0x01) return -1;   /* error */
            return 0;
        }
    }
    return -1;                           /* timeout */
}

int ata_read(uint32_t lba, void *dst, uint32_t sectors) {
    unsigned short *p = (unsigned short *)dst;
    if (!dst || !sectors) return -1;

    while (sectors) {
        unsigned int chunk = sectors > 255 ? 255 : sectors;

        if (wait_ready()) return -1;

        outb(ATA_DRIVE, (unsigned char)(0xE0 | ((lba >> 24) & 0x0F)));
        outb(ATA_SECNT, (unsigned char)chunk);
        outb(ATA_LBA0, (unsigned char)(lba & 0xFF));
        outb(ATA_LBA1, (unsigned char)((lba >> 8) & 0xFF));
        outb(ATA_LBA2, (unsigned char)((lba >> 16) & 0xFF));
        outb(ATA_CMD, 0x20);             /* read sectors with retries */

        for (unsigned int s = 0; s < chunk; s++) {
            if (wait_ready()) return -1;
            for (int i = 0; i < 256; i++) *p++ = inw(ATA_DATA);
        }

        lba += chunk;
        sectors -= chunk;
    }
    return 0;
}
