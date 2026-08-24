// SUB-OS Graphical User Interface Subsystem
#include <gui/gui.h>
#include <gui/gui_icons.h>
#include <gui/gui_dialog.h>
#include <gui/gui_apps_ext.h>
#include <kernel/printk.h>

void gui_init(void) {
    gui_icons_init();
    gui_dialog_init();
    printk(KERN_INFO "GUI: SUB-OS Desktop Environment ready "
                     "(SUB-WM compositor, SUB-WT widgets, %d icons)\n",
           GUI_ICON_COUNT);
}

int gui_start_desktop(void) {
    return gui_desktop_run();
}
