#ifndef _GUI_DIALOG_H
#define _GUI_DIALOG_H

#include <stdint.h>
#include <stdbool.h>
#include <gui/gui_icons.h>

// Modal dialogs. Only one is on screen at a time; while it is up the desktop
// dims behind it and the compositor routes every click to the dialog, so no
// window underneath can be interacted with.

typedef enum {
    GUI_DIALOG_INFO,
    GUI_DIALOG_WARNING,
    GUI_DIALOG_CONFIRM,
    GUI_DIALOG_ERROR
} gui_dialog_kind_t;

typedef enum {
    GUI_DIALOG_RESULT_NONE = 0,
    GUI_DIALOG_RESULT_OK,
    GUI_DIALOG_RESULT_CANCEL
} gui_dialog_result_t;

typedef void (*gui_dialog_cb_t)(gui_dialog_result_t result, void* ctx);

void gui_dialog_init(void);

void gui_dialog_show(gui_dialog_kind_t kind, const char* title, const char* message);
void gui_dialog_confirm(const char* title, const char* message,
                        gui_dialog_cb_t on_result, void* ctx);
void gui_dialog_dismiss(void);

bool gui_dialog_is_open(void);

// Returns true when the dialog consumed the input.
bool gui_dialog_handle_mouse(int mx, int my, bool clicked);
bool gui_dialog_handle_key(uint16_t key);

void gui_dialog_render(void);

#endif // _GUI_DIALOG_H
