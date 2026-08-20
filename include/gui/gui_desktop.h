#ifndef _GUI_DESKTOP_H
#define _GUI_DESKTOP_H

#include <stdint.h>
#include <stdbool.h>

#define GUI_TASKBAR_HEIGHT 20

void gui_desktop_init(void);
void gui_desktop_render_background(void);
void gui_desktop_render_taskbar(void);
void gui_desktop_render_start_menu(void);
void gui_desktop_toggle_start_menu(void);
bool gui_desktop_is_start_menu_open(void);
void gui_desktop_handle_taskbar_click(int mx, int my);

int gui_desktop_run(void);

#endif // _GUI_DESKTOP_H
