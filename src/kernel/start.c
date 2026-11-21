/*
 * Woodpile - 64-bit hobby OS
 * Boots into a framebuffer-backed text console and starts qnt.xec (the shell).
 */

#include "modules/vbe.h"
#include "modules/framebuffer.h"
#include "modules/time.h"
#include "modules/idt.h"
#include "modules/memtest.h"
#include "modules/initramfs.h"
#include "modules/serial.h"
#include "modules/kbd.h"
#include "modules/mouse.h"
#include "modules/vfs.h"
#include "modules/abi.h"
#include "modules/xec.h"
#include "modules/ata.h"

extern uint64_t initramfs_ptr;
extern uint64_t initramfs_size;
extern uint64_t initramfs_lba;

static uint64_t boot_mem_top(void) {
    uint64_t t = vbe_mem_top();
    return t ? t : (128ULL * 1024 * 1024);
}

static void map_framebuffer(uint64_t phys, uint64_t size) {
    /* PML4=0x20000, PDPT=0x21000, PD=0x22000.  Only the PDPT may be
     * extended here; writing these entries into the PML4 breaks the
     * kernel's higher-half mappings and resets the machine. */
    volatile uint64_t *pdp = (volatile uint64_t *)0x21000;

    uint64_t start = phys & ~0x1FFFFFULL;
    uint64_t end   = phys + size;

    for (uint64_t p = start; p < end; p += 0x200000) {
        uint16_t pdp_idx = (p >> 30) & 0x1FF;
        uint16_t pd_idx  = (p >> 21) & 0x1FF;

        volatile uint64_t *pd;
        if (pdp[pdp_idx] & 1) {
            uint64_t pd_phys = pdp[pdp_idx] & 0xFFFFFFFFFFFFF000ULL;
            pd = (volatile uint64_t *)(unsigned long)pd_phys;
        } else {
            pd = (volatile uint64_t *)0x70000;
            for (int i = 0; i < 512; i++) pd[i] = 0;
            pdp[pdp_idx] = 0x70003ULL;
        }

        pd[pd_idx] = (p & 0xFFFFFFFFFFFFF000ULL) | 0x83ULL;
    }

    asm volatile("mov %0, %%cr3" :: "r"(0x20000ULL) : "memory");
}

void kmain(void) {
    serial_init();
    serial_puts("WP: kmain start\n");

    idt_init();
    serial_puts("WP: idt ready\n");
    time_init();
    serial_puts("WP: pit ready\n");

    /* Set Resolution to GetResolution */
    uint16_t wanted_width, wanted_height;
    vbe_get_resolution(&wanted_width, &wanted_height);
    serial_puts("WP: VBE target ");
    serial_puthex(wanted_width);
    serial_puts("x");
    serial_puthex(wanted_height);
    serial_puts("\n");

    struct vbe_result vbe;
    if (vbe_set_resolution(wanted_width, wanted_height, &vbe) == 0) {
        serial_puts("WP: VBE ");
        serial_puthex(vbe.width);
        serial_puts("x");
        serial_puthex(vbe.height);
        serial_puts(" mode=0x");
        serial_puthex(vbe_best_mode_code());
        serial_puts(vbe_exact_match() ? " exact\n" : " closest\n");

        map_framebuffer(vbe.fb_addr, (uint64_t)vbe.pitch * vbe.height);
        framebuffer_init(&vbe);
    } else {
        serial_puts("WP: VBE failed trace=");
        serial_puthex(vbe_trace_code());
        serial_puts("\n");
        framebuffer_init(0);
    }

    fb_clear(fb_color(0, 0, 0));
    fb_setcolor(C_GRAY, C_BLACK);
    fb_console_clear();

    memtest(boot_mem_top());
    serial_puts("WP: memtest done\n");

    /* The BIOS cannot stage a multi-megabyte initramfs below 1 MiB, so
     * read it straight from the boot disk into high memory with ATA PIO. */
    serial_puts("WP: loading initramfs\n");
    /* ISO boot stages the initramfs via BIOS in the bootloader because ATA
     * PIO cannot read the ATAPI CD-ROM.  Direct HDD boot still uses ATA. */
    volatile uint32_t *irfs_magic =
        (volatile uint32_t *)(unsigned long)initramfs_ptr;
    if (*irfs_magic == 0x53465057) {
        serial_puts("WP: initramfs preloaded\n");
    } else {
        uint32_t iramfs_secs = (uint32_t)((initramfs_size + 511) / 512);
        if (ata_read((uint32_t)initramfs_lba,
                     (void *)(unsigned long)initramfs_ptr,
                     iramfs_secs) != 0) {
            serial_puts("WP: initramfs load failed\n");
        }
    }

    initramfs_init();
    vfs_init();
    abi_init();
    kbd_init();
    mouse_init();

    serial_puts("WP: initramfs files=");
    serial_puthex(initramfs_count());
    serial_puts("\n");

    /* The VBE BIOS call reprograms the PIT; restore our 1 kHz tick. */
    time_init();

    /* VBE trampoline enters real mode with CLI and never restores IF. */
    asm volatile("sti");

    xec_args_t args;
    args.argc = 1;
    args.reserved = 0;
    args.argv[0] = "sys32/bin/qnt.xec";
    for (int i = 1; i < 16; i++) args.argv[i] = 0;
    args.cwd = "sys32";

    xec_run("sys32/bin/qnt.xec", &args);

    serial_puts("WP: shell returned, halting\n");
    for (;;) asm volatile("hlt");
}
