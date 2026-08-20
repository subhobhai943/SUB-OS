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
#include <arch/arch.h>
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
#include <drivers/bochs.h>
#include <drivers/virtio_blk.h>
#include <drivers/virtio_net.h>
#include <drivers/virtio_rng.h>
#include <drivers/hwmon.h>
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
#include <net/dns_cache.h>
#include <ipc/shm_posix.h>
#include <sound/beep.h>
#include <kernel/tsc.h>
#include <drivers/e1000e.h>
#include <drivers/virtio_gpu.h>
#include <drivers/cpufreq.h>
#include <drivers/virtio_input.h>
#include <kernel/rust.h>
#include <kernel/sub_lang.h>
#include <kernel/cpp_kernel.h>
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

static void subos_modular_core_boot(void) {
#if defined(__x86_64__)
    pci_init();
#endif

    // Storage & Block Layer
    printk(KERN_INFO "[4/14] Initializing Block Layer & Storage Controllers... ");
    block_init();
    ramdisk_init();
#if defined(__x86_64__)
    ata_init();
    ahci_init();
    nvme_init();
#endif
    virtio_blk_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // Bus & Devices
    printk(KERN_INFO "[5/14] Enumerating System Devices, Canvas & PTY... ");
#if defined(__x86_64__)
    virt_init();
    fb_init();
    bochs_vbe_init();
    virtio_gpu_init();
    usb_init();
    xhci_init();
    hwmon_init();
    cpufreq_init();
#endif
    canvas_init();
    pty_init();
    virtio_rng_init();
    virtio_input_init();
    tsc_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // Network Subsystem, NetFilter, HTTPD & SSH Server
    printk(KERN_INFO "[6/14] Initializing Network Stack, NetFilter, HTTPD & SSHD... ");
    filter_init();
    httpd_init();
    sshd_init();
#if defined(__x86_64__)
    rtl8139_init();
    virtio_net_init();
    e1000e_init();
    if (e1000_init() || e1000e_is_online()) {
        net_init();
        socket_subsystem_init();
        udp_init();
        tcp_init();
        dhcp_init();
        dns_init();
        dns_cache_init();
        printk(ANSI_BRIGHT_GREEN "Online\n" ANSI_RESET);
    } else {
        printk(ANSI_YELLOW "No NIC detected\n" ANSI_RESET);
    }
#else
    virtio_net_init();
    dns_cache_init();
    printk(ANSI_BRIGHT_GREEN "Online\n" ANSI_RESET);
#endif

    // Sound & Voice Synthesizer
#if defined(__x86_64__)
    printk(KERN_INFO "[7/14] Initializing Sound Architecture, Melody Player & Formant TTS... ");
    sound_init();
    hda_init();
    melody_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);
#endif

    // Crypto, Certificates, LSM Security, Authentication, Namespaces, Rust, SUB-Lang & C++ Subsystems
    printk(KERN_INFO "[8/14] Initializing Rust, C++ Core, SUB-Lang Engine, Keyring & Auth... ");
    rust_kernel_init();
    cpp_kernel_init();
    sub_kernel_init();
    prng_seed(0, 0);
    certs_init();
    security_init();
    auth_init();
    namespace_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // Inter-Process Communication
    printk(KERN_INFO "[9/14] Initializing IPC Engine (Pipes, MsgQueues, POSIX SHM, Semaphores)... ");
    ipc_init();
    posix_shm_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // Asynchronous I/O Engine
    printk(KERN_INFO "[10/14] Initializing io_uring Async Ring Engine... ");
    io_uring_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // Virtual File System & Multi-Filesystem Support
    printk(KERN_INFO "[11/14] Mounting Virtual File System (VFS, devfs, procfs, sysfs)... ");
    vfs_init();
    sysfs_init();
    kobject_subsystem_init();
    fat32_init();
    ext2_init();
    initramfs_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // Kernel Core: Signals, Modules, Syscalls, Tracing, Syslog, Metrics, Crond & Systemd
    printk(KERN_INFO "[12/14] Initializing Syslog, Metrics, Cron, Tracing, BPF & Services... ");
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

    // Preemptive Multi-Tasking & Userland
    printk(KERN_INFO "[13/14] Initializing Preemptive Task Scheduler & LazyBox... ");
    sched_init();
    lazybox_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    // Hardware Interrupts
    printk(KERN_INFO "[14/14] Enabling Hardware Interrupts... ");
    arch_enable_interrupts();
    printk(ANSI_BRIGHT_GREEN "ACTIVE\n" ANSI_RESET);

    // Launch Shell
    shell_run();

    while (1) {
        arch_halt();
    }
}

#if defined(__x86_64__)
void kernel_main(void* memory_map, uint64_t memory_map_count) {
    tty_init();
    serial_init();
    printk_init();

    printk(ANSI_BRIGHT_CYAN "=================================================================\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "   SUB-OS 64-Bit [%s] Modular Monolithic Kernel %s\n" ANSI_RESET, arch_get_name(), kernel_get_version());
    printk(ANSI_BRIGHT_CYAN "=================================================================\n\n" ANSI_RESET);

    init_early("root=/dev/sda console=tty1 init=/bin/lazybox quiet");
    rtc_init();

    printk(KERN_INFO "[1/14] Initializing 64-Bit GDT, IDT, PIC & PIT Timer... ");
    arch_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[2/14] Initializing PS/2 Keyboard, Mouse & ACPI... ");
    keyboard_init();
    mouse_init();
    acpi_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[3/14] Initializing Physical PMM, Dynamic Heap & SLAB Cache... ");
    pmm_init(memory_map, memory_map_count);
    heap_init();
    slab_init();
    vma_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    subos_modular_core_boot();
}
#elif defined(__aarch64__)
void kernel_main_aarch64(void) {
    arch_early_init();
    printk_init();

    printk(ANSI_BRIGHT_CYAN "=================================================================\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "   SUB-OS 64-Bit [%s] Modular Monolithic Kernel %s\n" ANSI_RESET, arch_get_name(), kernel_get_version());
    printk(ANSI_BRIGHT_CYAN "=================================================================\n\n" ANSI_RESET);

    init_early("root=/dev/vda console=ttyAMA0 init=/bin/lazybox quiet");

    printk(KERN_INFO "[1/14] Initializing AArch64 CPU, GICv2, Generic Timer & MMU... ");
    arch_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[2/14] Initializing Physical PMM, Dynamic Heap & SLAB Cache... ");
    pmm_init(NULL, 0);
    heap_init();
    slab_init();
    vma_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    subos_modular_core_boot();
}
#elif defined(__arm__) || defined(__armv8i__)
void kernel_main_armv8i(void) {
    arch_early_init();
    printk_init();

    printk(ANSI_BRIGHT_CYAN "=================================================================\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "   SUB-OS 32-Bit [%s] Modular Monolithic Kernel %s\n" ANSI_RESET, arch_get_name(), kernel_get_version());
    printk(ANSI_BRIGHT_CYAN "=================================================================\n\n" ANSI_RESET);

    init_early("root=/dev/vda console=ttyAMA0 init=/bin/lazybox quiet");

    printk(KERN_INFO "[1/14] Initializing ARMv8i CPU, GICv2, Generic Timer & MMU... ");
    arch_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    printk(KERN_INFO "[2/14] Initializing Physical PMM, Dynamic Heap & SLAB Cache... ");
    pmm_init(NULL, 0);
    heap_init();
    slab_init();
    vma_init();
    printk(ANSI_BRIGHT_GREEN "OK\n" ANSI_RESET);

    subos_modular_core_boot();
}
#endif
