#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

typedef struct {
    int x;
    int y;
    int left;
    int right;
    int middle;
    int dx;
    int dy;
} mouse_state_t;

void mouse_init(void);
void mouse_poll(void);
mouse_state_t *mouse_get(void);

void mouse_draw_cursor(void);
void mouse_erase_cursor(void);
void mouse_irq_handler(void);

#endif