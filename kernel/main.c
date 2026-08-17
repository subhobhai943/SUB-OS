#include <kernel/kernel.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/task.h>
#include <kernel/signal.h>
#include <kernel/syscall.h>
#include <kernel/timer.h>
#include <kernel/workqueue.h>
#include <kernel/module.h>
#include <kernel/trace.h>
#include <kernel/namespace.h>
#include <kernel/syslog.h>
#include <kernel/cron.h>
#include <kernel/metrics.h>
#include <kernel/bpf.h>
#include <kernel/kobject.h>
#include <mm/vma.h>
#include <drivers/canvas.h>
#include <drivers/pty.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/isr.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/cpuid.h>
#include <arch/x86_64/paging.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <mm/slab.h>
#include <drivers/tty.h>
#include <drivers/serial.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/ata.h>
#include <drivers/ahci.h>
#include <drivers/nvme.h>
#include <drivers/pci.h>
#include <drivers/e1000.h>
#include <drivers/rtl8139.h>
#include <drivers/speaker.h>
#include <drivers/fb.h>
#include <drivers/rtc.h>
#include <drivers/acpi.h>
#include <drivers/usb.h>
#include <drivers/xhci.h>
#include <drivers/ramdisk.h>
#include <block/block.h>
#include <ipc/ipc.h>
#include <sound/sound.h>
#include <sound/hda.h>
#include <sound/melody.h>
#include <fs/vfs.h>
#include <fs/fat32.h>
#include <fs/ext2.h>
#include <fs/sysfs.h>
#include <net/net.h>
#include <net/socket.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/dhcp.h>
#include <net/dns.h>
#include <net/filter.h>
#include <net/http.h>
#include <net/ssh.h>
#include <crypto/crypto.h>
#include <certs/certs.h>
#include <security/security.h>
#include <security/auth.h>
#include <io_uring/io_uring.h>
#include <usr/initramfs.h>
#include <virt/virt.h>
#include <init/init.h>
#include <init/service.h>
#include <init/version.h>
#include <userland/lazybox.h>
#include <userland/sh.h>
#include <userland/shell.h>

void kernel_main(void* memory_map, uint64_t memory_map_count) {
    // 1. Console & Telemetry
    tty_init();
    serial_init();
    printk_init();

    printk(ANSI_BRIGHT_CYAN "=================================================================\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "   SUB-OS 64-Bit Production Modular Monolithic Kernel %s\n" ANSI_RESET, kernel_get_version());
    printk(ANSI_BRIGHT_CYAN "=================================================================\n\n" ANSI_RESET);

    // 2. Early Boot Parameters & RTC Real-Time Clock
    init_early("root=/dev/sda console=tty1 init=/bin/lazybox quiet");
    rtc_init();

    // 3. CPU Architecture & Interrupts
    printk(KERN_INFO "[1/18] Initializing 64-Bit GDT & TSS... ");
    gdt_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[2/18] Initializing Interrupt Descriptor Table (IDT & ISRs)... ");
    idt_init();
    isr_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[3/18] Remapping Dual 8259A PIC Controller... ");
    pic_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[4/18] Configuring 8254 PIT Timer & High-Res Timer Wheel... ");
    pit_init(100);
    timer_wheel_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 4. Input & Power Drivers
    printk(KERN_INFO "[5/18] Initializing PS/2 Keyboard, Mouse & ACPI... ");
    keyboard_init();
    mouse_init();
    acpi_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 5. Memory Management: PMM + Dynamic Heap + SLAB Cache
    printk(KERN_INFO "[6/18] Initializing Physical PMM, Dynamic Heap & SLAB Cache... ");
    pmm_init(memory_map, memory_map_count);
    heap_init();
    slab_init();
    vma_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 6. Storage & Block Layer
    printk(KERN_INFO "[7/18] Initializing Block Layer, Ramdisk & Storage Controllers... ");
    block_init();
    ramdisk_init();
    ata_init();
    ahci_init();
    nvme_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 7. Bus & Devices
    printk(KERN_INFO "[8/18] Enumerating PCI Bus, VirtIO, xHCI, Canvas & PTY... ");
    pci_init();
    virt_init();
    fb_init();
    canvas_init();
    pty_init();
    usb_init();
    xhci_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 7. Network Subsystem, NetFilter, HTTPD & SSH Server
    printk(KERN_INFO "[9/18] Initializing Network Stack, NetFilter, HTTPD & SSHD... ");
    filter_init();
    httpd_init();
    sshd_init();
    rtl8139_init();
    if (e1000_init()) {
        net_init();
        socket_subsystem_init();
        udp_init();
        tcp_init();
        dhcp_init();
        dns_init();
        printk(ANSI_BRIGHT_GREEN "Online\n" ANSI_RESET);
    } else {
        printk(ANSI_YELLOW "No NIC detected\n" ANSI_RESET);
    }

    // 8. Sound & Voice Synthesizer
    printk(KERN_INFO "[10/18] Initializing Sound Architecture, Melody Player & Formant TTS... ");
    sound_init();
    hda_init();
    melody_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 9. Crypto, Certificates, LSM Security, Authentication & Namespaces
    printk(KERN_INFO "[11/18] Initializing X.509 Keyring, Shadow Auth & Namespaces... ");
    prng_seed(0, 0);
    certs_init();
    security_init();
    auth_init();
    namespace_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 10. Inter-Process Communication
    printk(KERN_INFO "[12/18] Initializing IPC Engine (Pipes, MsgQueues, SHM, Semaphores)... ");
    ipc_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 11. Asynchronous I/O Engine
    printk(KERN_INFO "[13/18] Initializing io_uring Async Ring Engine... ");
    io_uring_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 12. Virtual File System & Multi-Filesystem Support
    printk(KERN_INFO "[14/18] Mounting Virtual File System (VFS, devfs, procfs, sysfs)... ");
    vfs_init();
    sysfs_init();
    kobject_subsystem_init();
    fat32_init();
    ext2_init();
    initramfs_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 13. Kernel Core: Signals, Modules, Syscalls, Tracing, Syslog, Metrics, Crond & Systemd
    printk(KERN_INFO "[15/18] Initializing Syslog, Metrics, Cron, Tracing, BPF & Services... ");
    signal_init();
    workqueue_init();
    syscall_init();
    module_init_subsystem();
    trace_init();
    bpf_init();
    syslog_init();
    metrics_init();
    cron_init();
    service_manager_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 14. Preemptive Multi-Tasking & Userland
    printk(KERN_INFO "[16/18] Initializing Preemptive Task Scheduler & LazyBox... ");
    sched_init();
    lazybox_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // 15. Hardware Interrupts
    printk(KERN_INFO "[17/18] Enabling Hardware Interrupts... ");
    sti();
    printk(ANSI_BRIGHT_GREEN "ACTIVE\n" ANSI_RESET);

    // 16. Welcome & Launch Shell
    printk(KERN_INFO "[18/18] Launching Userland Shell on TTY1... ");
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    tty_clear();
    shell_run();

    while (1) {
        hlt();
    }
}
