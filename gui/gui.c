// SUB-OS Graphical User Interface Subsystem
#include <gui/gui.h>
#include <kernel/printk.h>

void gui_init(void) {
    printk(KERN_INFO "GUI: SUB-OS Graphical Desktop Environment initialized (Ready)\n");
}

int gui_start_desktop(void) {
    return gui_desktop_run();
}
