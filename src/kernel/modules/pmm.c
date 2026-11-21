#include "pmm.h"

#define PMM_PAGE_SIZE  4096
#define PMM_MAX_PAGES  32768
#define PMM_RESERVED   256

extern unsigned char bss_end[];

static uint8_t bitmap[PMM_MAX_PAGES / 8];
static uint64_t mem_top;
static uint64_t pmm_reserved = PMM_RESERVED;

static inline void mark_used(uint64_t p) {
    bitmap[p / 8] |= (1 << (p % 8));
}

static inline void mark_free(uint64_t p) {
    bitmap[p / 8] &= ~(1 << (p % 8));
}

static inline int is_used(uint64_t p) {
    return (bitmap[p / 8] >> (p % 8)) & 1;
}

void pmm_init(uint64_t mem_top_) {
    mem_top = mem_top_;
    for (int i = 0; i < sizeof(bitmap); i++)
        bitmap[i] = 0xFF;

    /* Kernel BSS begins at 1 MiB; never hand those pages to kalloc. */
    uint64_t bss_pages =
        ((uint64_t)(unsigned long)bss_end + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    pmm_reserved = bss_pages > PMM_RESERVED ? bss_pages : PMM_RESERVED;

    uint64_t end = mem_top / PMM_PAGE_SIZE;
    if (end > PMM_MAX_PAGES) end = PMM_MAX_PAGES;
    for (uint64_t p = pmm_reserved; p < end; p++)
        mark_free(p);
}

uint64_t pmm_alloc_page(void) {
    for (uint64_t p = pmm_reserved; p < PMM_MAX_PAGES; p++)
        if (!is_used(p)) {
            mark_used(p);
            return p * PMM_PAGE_SIZE;
        }
    return 0;
}

void pmm_free_page(uint64_t addr) {
    uint64_t p = addr / PMM_PAGE_SIZE;
    if (p >= pmm_reserved && p < PMM_MAX_PAGES)
        mark_free(p);
}
