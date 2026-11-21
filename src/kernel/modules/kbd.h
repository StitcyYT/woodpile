#ifndef KBD_H
#define KBD_H

void kbd_init(void);
void kbd_handler(void);

/* Blocking line reader with echo and backspace editing. */
int kbd_getline(char *buf, int max);

/* Blocking single-character reader (used by the pager). */
int kbd_getc(void);

/* Non-blocking raw events for GUI sessions.
 * bits 0..7   scancode
 * bit 8       key released
 * bit 9       shift held
 * bit 10      ctrl held
 */
int kbd_scancode_nb(void);

/* Drop queued raw events so a new program doesn't inherit the keys used to
 * type its command. */
void kbd_flush(void);

#endif
