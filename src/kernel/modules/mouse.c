#include "mouse.h"
#include "framebuffer.h"
#include "serial.h"

static int mouse_x, mouse_y, mouse_buttons;
static unsigned char packet[3];
static int packet_index;

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline unsigned char inb(unsigned short port) {
    unsigned char v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void io_wait(void) {
    outb(0x80, 0);
}

static void wait_input(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 0x02)) return;
}
static void wait_output(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 0x01) return;
}
static unsigned char mouse_read(void) {
    wait_output();
    return inb(0x60);
}
static unsigned char mouse_write(unsigned char value) {
    wait_input();
    outb(0x64, 0xD4);
    wait_input();
    outb(0x60, value);
    return mouse_read();         /* consume 0xFA / 0xFE ack */
}

void mouse_init(void) {
    wait_input();
    outb(0x64, 0xAD);            /* disable keyboard */
    wait_input();
    outb(0x64, 0xA8);            /* enable auxiliary device */

    wait_input();
    outb(0x64, 0x20);            /* read controller config */
    unsigned char config = mouse_read();
    wait_input();
    outb(0x64, 0x60);            /* write controller config */
    wait_input();
    outb(0x60, (unsigned char)(config | 0x02));  /* enable IRQ12 */

    (void)mouse_write(0xF6);         /* set default settings */
    (void)mouse_write(0xF4);         /* enable streaming */

    wait_input();
    outb(0x64, 0xAE);            /* re-enable keyboard */

    /* Drop any stale bytes (ACKs) left in the controller output buffer. */
    for (int i = 0; i < 16 && (inb(0x64) & 0x01); i++)
        (void)inb(0x60);

    /* `make run` warps the host pointer to 0,0 and boots QEMU fullscreen,
     * so the guest cursor should start at the same origin.  Otherwise the
     * first real motion packet lands one screen-center offset away. */
    mouse_x = 0;
    mouse_y = 0;
    packet_index = 0;

    serial_puts("[mouse] ps/2 ready\n");
}

void mouse_handler(void) {
    unsigned char byte = inb(0x60);

    if (packet_index == 0 && !(byte & 0x08)) {
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    packet[packet_index++] = byte;
    if (packet_index == 3) {
        int dx = (packet[0] & 0x10) ? (int)packet[1] - 256 : (int)packet[1];
        int dy = (packet[0] & 0x20) ? (int)packet[2] - 256 : (int)packet[2];

        mouse_buttons = (packet[0] & 0x01) ? 1 : 0;
        if (packet[0] & 0x02) mouse_buttons |= 2;

        mouse_x += dx;
        mouse_y -= dy;           /* PS/2 positive Y means "up" */

        int w = fb_width(), h = fb_height();
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= w) mouse_x = w - 1;
        if (mouse_y >= h) mouse_y = h - 1;

        packet_index = 0;
    }

    outb(0xA0, 0x20);            /* EOI to slave PIC */
    outb(0x20, 0x20);            /* EOI to master PIC */
}

void mouse_get(int *x, int *y) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}

void mouse_state(int *x, int *y, int *buttons) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
    if (buttons) *buttons = mouse_buttons;
}

/* Start every GUI session with the pointer at the top-left corner, matching
 * the host pointer warp in `make run`.  Nested gfx_enter() calls from child
 * apps don't reset it because fb_gui_enter() only does this at depth 0. */
void mouse_set_pos(int x, int y) {
    int w = fb_width(), h = fb_height();
    mouse_x = x < 0 ? 0 : (x >= w ? w - 1 : x);
    mouse_y = y < 0 ? 0 : (y >= h ? h - 1 : y);
    mouse_buttons = 0;
    packet_index = 0;
}
