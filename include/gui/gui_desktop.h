#ifndef _GUI_DESKTOP_H
#define _GUI_DESKTOP_H

#include <stdint.h>
#include <stdbool.h>

#define GUI_TASKBAR_HEIGHT   32
#define GUI_DESKTOP_ICON_W   72
#define GUI_DESKTOP_ICON_H   64

void gui_desktop_init(void);

void gui_desktop_render_background(void);

// Drop the cached wallpaper so the next background render repaints it. Needed
// when the resolution changes or the framebuffer is handed to another owner.
void gui_desktop_invalidate_background(void);
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

// An app that animates continuously calls this from its paint routine to ask
// for a full-rate recomposite instead of the ~10 Hz live-content heartbeat.
// The request covers only the next frame, so it has to be renewed on every
// paint; it therefore lapses by itself when the app stops animating or its
// window closes, and the compositor goes straight back to idling cheaply.
void gui_desktop_request_animation_frame(void);

// Wallpaper grid overlay (also toggled from the desktop context menu).
void gui_desktop_set_grid(bool on);
bool gui_desktop_get_grid(void);

// Taskbar clock: show seconds (HH:MM:SS) instead of HH:MM.
void gui_desktop_set_clock_seconds(bool on);
bool gui_desktop_get_clock_seconds(void);

void gui_desktop_request_exit(void);
int  gui_desktop_run(void);

#endif // _GUI_DESKTOP_H
