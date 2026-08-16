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
#include <certs/certs.h>
#include <security/security.h>
#include <io_uring/io_uring.h>
#include <usr/initramfs.h>
#include <virt/virt.h>
#include <init/init.h>
#include <init/version.h>
#include <userland/lazybox.h>
#include <userland/shell.h>

void kernel_main(void* memory_map, uint64_t memory_map_count) {
    // 1. Initialize Console & Telemetry Serial Port
    tty_init();
    serial_init();
    printk_init();

    printk(ANSI_BRIGHT_CYAN "=================================================================\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "   SUB-OS 64-Bit Production Monolithic Kernel %s\n" ANSI_RESET, kernel_get_version());
    printk(ANSI_BRIGHT_CYAN "=================================================================\n\n" ANSI_RESET);

    // Early Boot Parameters
    init_early("root=/dev/sda console=tty1 init=/bin/lazybox quiet");

    // 2. CPU Architecture & Interrupt Initialization
    printk(KERN_INFO "[1/14] Initializing 64-Bit Global Descriptor Table (GDT & TSS)... ");
    gdt_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[2/14] Initializing Interrupt Descriptor Table (256 IDT Gates & ISRs)... ");
    idt_init();
    isr_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[3/14] Remapping Dual 8259A PIC Interrupt Controller... ");
    pic_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[4/14] Configuring 8254 PIT Timer (100 Hz, 10ms resolution)... ");
    pit_init(100);
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[5/14] Initializing PS/2 Keyboard Controller Driver... ");
    keyboard_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 3. Memory Subsystems
    printk(KERN_INFO "[6/14] Initializing Physical Memory Manager (PMM)...\n");
    pmm_init(memory_map, memory_map_count);

    printk(KERN_INFO "[7/14] Initializing Kernel Dynamic Heap Allocator (4 MB Pool)... ");
    heap_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    paging_init();

    // 4. Hardware Storage, PCI Bus, & Virtualization
    printk(KERN_INFO "[8/14] Initializing ATA / IDE Hard Disk Controller... ");
    if (ata_init()) {
        printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);
    } else {
        printk(ANSI_YELLOW "No ATA Drive\n" ANSI_RESET);
    }

    printk(KERN_INFO "[9/14] Enumerating PCI Bus, VirtIO & Network Device Drivers...\n");
    pci_init();
    virt_init();
    if (e1000_init()) {
        printk(ANSI_BRIGHT_GREEN "       Intel E1000 Gigabit Ethernet Controller Active\n" ANSI_RESET);
        net_init();
        printk(ANSI_BRIGHT_GREEN "       IPv4 / ARP / ICMP Network Protocol Stack Online\n" ANSI_RESET);
    } else {
        printk(ANSI_YELLOW "       No PCI Ethernet Controller detected\n" ANSI_RESET);
    }

    // 5. Security, Certificates & Cryptography Engine
    prng_seed(0, 0);
    printk(KERN_INFO "[10/14] Loading X.509 Keyring & Linux Security Module (LSM)... ");
    certs_init();
    security_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 6. Asynchronous I/O Engine
    printk(KERN_INFO "[11/14] Initializing io_uring Asynchronous I/O Engine... ");
    io_uring_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 7. Virtual File System & Initramfs
    printk(KERN_INFO "[12/14] Mounting Virtual File System (VFS, devfs, procfs)... ");
    vfs_init();
    initramfs_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 8. Multi-Tasking & Userland
    printk(KERN_INFO "[13/14] Initializing Preemptive Task Scheduler & LazyBox... ");
    sched_init();
    lazybox_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 9. Enable Hardware Interrupts
    printk(KERN_INFO "[14/14] Enabling Hardware Interrupts... ");
    sti();
    printk(ANSI_BRIGHT_GREEN "ACTIVE\n" ANSI_RESET);

    tty_clear();

    // 10. Launch Interactive Userland Shell
    shell_run();

    while (1) {
        hlt();
    }
}
