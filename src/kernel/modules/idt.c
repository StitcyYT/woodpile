#include "idt.h"
#include "serial.h"
#include "panic.h"
#include "time.h"
#include "kbd.h"
#include "mouse.h"
#include "framebuffer.h"

static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

void idt_set_entry(int num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_mid  = (base >> 16) & 0xFFFF;
    idt[num].base_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].sel = sel;
    idt[num].ist = 0;
    idt[num].flags = flags;
    idt[num].zero = 0;
}

static const char *exc_names[] = {
    "DE", "DB", "NMI", "BP", "OF", "BR", "UD", "NM",
    "DF", "CSO", "TS", "NP", "SS", "GP", "PF", "RES",
    "MF", "AC", "MC", "XF", "VE", "CP", "HV", "VC",
    "SX", "WX", "??", "??", "??", "??", "S?" };

static const char *exc_reasons[] = {
    "division by zero", "debug trap", "non-maskable interrupt", "breakpoint",
    "overflow", "bound range exceeded", "invalid opcode", "no math coprocessor",
    "double fault", "coprocessor segment overrun", "invalid task state",
    "segment not present", "stack segment fault", "general protection fault",
    "page fault", "reserved exception", "floating point error",
    "alignment check", "machine check", "SIMD floating point error",
    "virtualization exception", "control protection exception",
    "hypervisor injection exception", "VMM communication exception",
    "security exception", "unknown exception", "unknown exception",
    "unknown exception", "unknown exception", "unknown exception",
    "unknown exception", "unknown exception" };

static void exc_handler(regs_t *r) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    serial_puts("SERIAL: EXCEPTION ");
    if (r->int_no < 32) serial_puts(exc_names[r->int_no]);
    else serial_puts("???");
    serial_puts(" err=0x");
    serial_puthex(r->err_code);
    serial_puts(" rip=0x");
    serial_puthex(r->rip);
    serial_puts(" cr2=0x");
    serial_puthex(cr2);
    serial_puts("\n");

    const char *reason = (r->int_no < 32) ? exc_reasons[r->int_no]
                                          : "unknown exception";
    /* A double fault or stack fault may mean the stack/context is broken;
     * skip the full framebuffer painter and use the tiny serial-first path. */
    if (r->int_no == 8 || r->int_no == 12)
        panic_severe(reason, r->err_code, r->rip);

    /* For a page fault the faulting address is the most useful dump value. */
    uint64_t dump = (r->int_no == 14) ? cr2 : r->rip;
    panic(reason, r->err_code, dump);
}

void isr_handler(regs_t *r) {
    exc_handler(r);
}

static void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void timer_handler(regs_t *r) {
    (void)r;
    time_tick();
    fb_cursor_blink();
    outb(0x20, 0x20);
}

static void keyboard_handler(regs_t *r) {
    (void)r;
    kbd_handler();   /* sends its own EOI */
}

void irq_handler(regs_t *r) {
    switch (r->int_no) {
        case 32: timer_handler(r); break;
        case 33: keyboard_handler(r); break;
        case 44: mouse_handler(); break;
        default:
            serial_puts("SERIAL: spurious IRQ ");
            serial_puthex(r->int_no - 32);
            serial_puts("\n");
            outb(0x20, 0x20);
            break;
    }
}

static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xF8);   /* unmask IRQ0 (PIT), IRQ1 (keyboard), IRQ2 (cascade) */
    outb(0xA1, 0xEF);   /* IRQ12 (PS/2 mouse) */
}

void idt_init(void) {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;

    for (int i = 0; i < 256; i++)
        idt_set_entry(i, 0, 0, 0);

    void (*isrs[32])(void) = {
        isr_0, isr_1, isr_2, isr_3, isr_4, isr_5, isr_6, isr_7,
        isr_8, isr_9, isr_10, isr_11, isr_12, isr_13, isr_14, isr_15,
        isr_16, isr_17, isr_18, isr_19, isr_20, isr_21, isr_22, isr_23,
        isr_24, isr_25, isr_26, isr_27, isr_28, isr_29, isr_30, isr_31
    };

    void (*irqs[16])(void) = {
        irq_0, irq_1, irq_2, irq_3, irq_4, irq_5, irq_6, irq_7,
        irq_8, irq_9, irq_10, irq_11, irq_12, irq_13, irq_14, irq_15
    };

    for (int i = 0; i < 32; i++)
        idt_set_entry(i, (uint64_t)isrs[i], 0x08, 0x8E);

    for (int i = 0; i < 16; i++)
        idt_set_entry(i + 32, (uint64_t)irqs[i], 0x08, 0x8E);

    pic_remap();
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}
