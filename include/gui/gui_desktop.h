#ifndef _GUI_DESKTOP_H
#define _GUI_DESKTOP_H

#include <stdint.h>
#include <stdbool.h>

#define GUI_TASKBAR_HEIGHT   32
#define GUI_DESKTOP_ICON_W   72
#define GUI_DESKTOP_ICON_H   64

void gui_desktop_init(void);

void gui_desktop_render_background(void);
void gui_desktop_render_icons(void);
void gui_desktop_render_taskbar(void);
void gui_desktop_render_start_menu(void);
void gui_desktop_render_context_menu(void);

void gui_desktop_toggle_start_menu(void);
bool gui_desktop_is_start_menu_open(void);
void gui_desktop_open_context_menu(int mx, int my);
void gui_desktop_close_menus(void);

// Returns true if the click was consumed by desktop chrome (taskbar, menus,
// icons) rather than falling through to the window manager.
bool gui_desktop_handle_click(int mx, int my);

// Height of the area windows may occupy (screen minus taskbar).
int  gui_desktop_workarea_height(void);

void gui_desktop_request_exit(void);
int  gui_desktop_run(void);

#endif // _GUI_DESKTOP_H
