#ifndef _GUI_CURSOR_H
#define _GUI_CURSOR_H

#include <stdint.h>
#include <stdbool.h>

void gui_cursor_init(void);
void gui_cursor_set_pos(int x, int y);
int gui_cursor_get_x(void);
int gui_cursor_get_y(void);
void gui_cursor_draw(void);

// Overlay compositing: restore the pixels under the previous cursor position,
// then stash-and-draw the cursor at the current one. Lets the compositor move
// the pointer at full frame rate without repainting the whole scene.
void gui_cursor_restore_under(void);
void gui_cursor_composite(void);

#endif // _GUI_CURSOR_H
