#include "wpabi.h"

/* First module launched by the kernel. It brings up the desktop module. */
void module_entry(void) {
    serial_puts("run64.co: launching att.co\n");
    fb_drawstr(12, 12, "WOODPILE OS 64",
               fb_color(255, 255, 255), fb_color(0, 128, 128));
    module_run_path("sys32/att.co");
    serial_puts("run64.co: att.co returned\n");
}
