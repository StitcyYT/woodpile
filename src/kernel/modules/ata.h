#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* Read sectors from the primary IDE master using PIO LBA28. */
int ata_read(uint32_t lba, void *dst, uint32_t sectors);

#endif
