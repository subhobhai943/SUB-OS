#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/task.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/isr.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/cpuid.h>
#include <arch/x86_64/paging.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <drivers/tty.h>
#include <drivers/serial.h>
#include <drivers/keyboard.h>
#include <drivers/ata.h>
#include <drivers/pci.h>
#include <drivers/e1000.h>
#include <drivers/speaker.h>
#include <fs/vfs.h>
#include <net/net.h>
#include <crypto/crypto.h>
#include <userland/lazybox.h>
#include <userland/shell.h>

void kernel_main(void* memory_map, uint64_t memory_map_count) {
    // 1. Initialize Console & Telemetry Serial Port
    tty_init();
    serial_init();
    printk_init();

    printk(ANSI_BRIGHT_CYAN "=================================================================\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "   SUB-OS 64-Bit Production Monolithic Kernel v%s (%s)\n" ANSI_RESET, KERNEL_VERSION, KERNEL_ARCH);
    printk(ANSI_BRIGHT_CYAN "=================================================================\n\n" ANSI_RESET);

    // 2. CPU Architecture & Interrupt Initialization
    printk(KERN_INFO "[1/11] Initializing 64-Bit Global Descriptor Table (GDT)... ");
    gdt_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[2/11] Initializing Interrupt Descriptor Table (IDT & ISRs)... ");
    idt_init();
    isr_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[3/11] Remapping Dual 8259A PIC Interrupt Controller... ");
    pic_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[4/11] Configuring 8254 PIT Timer (100 Hz, 10ms resolution)... ");
    pit_init(100);
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[5/11] Initializing PS/2 Keyboard Controller Driver... ");
    keyboard_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 3. Memory Subsystems
    printk(KERN_INFO "[6/11] Initializing Physical Memory Manager (PMM)...\n");
    pmm_init(memory_map, memory_map_count);

    printk(KERN_INFO "[7/11] Initializing Kernel Dynamic Heap Allocator (4 MB Pool)... ");
    heap_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    paging_init();

    // 4. Hardware Storage & Bus Enumeration
    printk(KERN_INFO "[8/11] Initializing ATA / IDE Hard Disk Controller... ");
    if (ata_init()) {
        printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);
    } else {
        printk(ANSI_YELLOW "No ATA Drive\n" ANSI_RESET);
    }

    printk(KERN_INFO "[9/11] Enumerating PCI Bus & Network Device Drivers...\n");
    pci_init();
    if (e1000_init()) {
        printk(ANSI_BRIGHT_GREEN "       Intel E1000 Gigabit Ethernet Controller Active\n" ANSI_RESET);
        net_init();
        printk(ANSI_BRIGHT_GREEN "       IPv4 / ARP / ICMP Network Protocol Stack Online\n" ANSI_RESET);
    } else {
        printk(ANSI_YELLOW "       No PCI Ethernet Controller detected\n" ANSI_RESET);
    }

    // 5. Security & Cryptography Engine
    prng_seed(0, 0);

    // 6. Virtual File System & Task Scheduler
    printk(KERN_INFO "[10/11] Mounting Virtual File System (VFS, devfs, procfs)... ");
    vfs_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[11/11] Initializing Preemptive Task Scheduler & LazyBox... ");
    sched_init();
    lazybox_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 7. Enable Hardware Interrupts
    sti();
    printk(ANSI_BRIGHT_GREEN "[INIT] Hardware interrupts active. System ready for userland!\n" ANSI_RESET);

    tty_clear();

    // 8. Launch Interactive Userland Shell
    shell_run();

    while (1) {
        hlt();
    }
}
