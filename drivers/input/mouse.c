#include <drivers/mouse.h>
#include <arch/x86_64/io.h>
#include <kernel/printk.h>

static mouse_state_t mouse_state = {0, 0, false, false, false};
static int mouse_bounds_x = 80;
static int mouse_bounds_y = 25;

void mouse_init(void) {
    mouse_state.x = mouse_bounds_x / 2;
    mouse_state.y = mouse_bounds_y / 2;
    printk(KERN_INFO "MOUSE: PS/2 Mouse Controller initialized (Bounds: %dx%d)\n",
           mouse_bounds_x, mouse_bounds_y);
}

const mouse_state_t* mouse_get_state(void) {
    return &mouse_state;
}

void mouse_set_bounds(int max_x, int max_y) {
    mouse_bounds_x = max_x;
    mouse_bounds_y = max_y;
}
