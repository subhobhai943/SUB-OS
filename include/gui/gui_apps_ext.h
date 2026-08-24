#ifndef _GUI_APPS_EXT_H
#define _GUI_APPS_EXT_H

#include <gui/gui_wm.h>

// Second-generation desktop applications, built on the SUB-WT widget toolkit
// rather than raw framebuffer calls.

void gui_app_settings_launch(int x, int y, int w, int h);
void gui_app_taskmgr_launch(int x, int y, int w, int h);
void gui_app_logviewer_launch(int x, int y, int w, int h);
void gui_app_ktest_launch(int x, int y, int w, int h);
void gui_app_clock_launch(int x, int y, int w, int h);
void gui_app_editor_launch(int x, int y, int w, int h);

#endif // _GUI_APPS_EXT_H
