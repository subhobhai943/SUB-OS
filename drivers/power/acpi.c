#include <drivers/acpi.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/pit.h>
#include <kernel/printk.h>

void acpi_init(void) {
    printk(KERN_INFO "ACPI: Advanced Configuration & Power Interface subsystem initialized\n");
}

void acpi_poweroff(void) {
    printk(KERN_INFO "ACPI: Entering S5 Soft-Off State...\n");
    pit_sleep(100);
    outw(0x604, 0x2000);  // QEMU Poweroff
    outw(0xB004, 0x2000); // Bochs Poweroff
    outw(0x4004, 0x3400); // VirtualBox Poweroff
}

void acpi_reboot(void) {
    printk(KERN_INFO "ACPI: Triggering system reset...\n");
    pit_sleep(100);
    outb(0x64, 0xFE); // 8042 Keyboard controller pulse
    outb(0xCF9, 0x06); // PCI reset
}
