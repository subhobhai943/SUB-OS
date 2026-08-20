#ifndef _GUI_CURSOR_H
#define _GUI_CURSOR_H

#include <stdint.h>
#include <stdbool.h>

void gui_cursor_init(void);
void gui_cursor_set_pos(int x, int y);
int gui_cursor_get_x(void);
int gui_cursor_get_y(void);
void gui_cursor_draw(void);

#endif // _GUI_CURSOR_H
