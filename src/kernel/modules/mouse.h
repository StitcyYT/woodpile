#ifndef MOUSE_H
#define MOUSE_H

void mouse_init(void);
void mouse_handler(void);
void mouse_get(int *x, int *y);
void mouse_state(int *x, int *y, int *buttons);
void mouse_set_pos(int x, int y);

#endif
