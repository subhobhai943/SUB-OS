#ifndef _GUI_TERMINAL_H
#define _GUI_TERMINAL_H

#include <gui/gui_wm.h>

// Desktop terminal emulator. Commands are dispatched to the real LazyBox
// applet table, and the applet's printk output is captured and rendered in
// the window, so the GUI terminal runs the same tools the kernel TTY does.

void gui_app_terminal_launch(int x, int y, int w, int h);

#endif // _GUI_TERMINAL_H
