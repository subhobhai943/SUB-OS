#ifndef _DRIVERS_MOUSE_H
#define _DRIVERS_MOUSE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int x;
    int y;
    bool left_btn;
    bool right_btn;
    bool middle_btn;
} mouse_state_t;

void mouse_init(void);
const mouse_state_t* mouse_get_state(void);
void mouse_set_bounds(int max_x, int max_y);

#endif // _DRIVERS_MOUSE_H
