#include "kalloc.h"
#include "pmm.h"

#define ALIGN4 __attribute__((aligned(4)))

typedef struct block {
    size_t size;
    struct block *next;
} block_t;

static block_t *free_list;

static void add_block(void *addr, size_t size) {
    block_t *b = (block_t *)addr;
    b->size = size;
    b->next = free_list;
    free_list = b;
}

void kalloc_init(void) {
    free_list = 0;
    for (int i = 0; i < 16; i++) {
        uint64_t pg = pmm_alloc_page();
        if (pg) add_block((void *)pg, 4096);
    }
}

void *kalloc(size_t size) {
    if (size == 0) return 0;
    if (size < 8) size = 8;
    size = (size + 3) & ~3;
    size += sizeof(block_t);

    block_t **prev = &free_list;
    block_t *curr = free_list;

    while (curr) {
        if (curr->size >= size) {
            if (curr->size >= size + sizeof(block_t) + 4) {
                block_t *newb = (block_t *)((char *)curr + size);
                newb->size = curr->size - size;
                newb->next = curr->next;
                *prev = newb;
                curr->size = size;
            } else {
                *prev = curr->next;
            }
            curr->next = 0;
            return (char *)curr + sizeof(block_t);
        }
        prev = &curr->next;
        curr = curr->next;
    }

    uint64_t pg = pmm_alloc_page();
    if (!pg) return 0;
    add_block((void *)pg, 4096);
    return kalloc(size - sizeof(block_t));
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_t *b = (block_t *)((char *)ptr - sizeof(block_t));
    b->next = free_list;
    free_list = b;
}

void *krealloc(void *ptr, size_t size) {
    if (!ptr) return kalloc(size);
    if (size == 0) { kfree(ptr); return 0; }
    block_t *b = (block_t *)((char *)ptr - sizeof(block_t));
    void *newp = kalloc(size);
    if (newp) {
        size_t old = b->size - sizeof(block_t);
        for (size_t i = 0; i < (old < size ? old : size); i++)
            ((char *)newp)[i] = ((char *)ptr)[i];
        kfree(ptr);
    }
    return newp;
}
