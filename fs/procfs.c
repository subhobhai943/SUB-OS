#include <fs/procfs.h>
#include <fs/ramfs.h>
#include <arch/x86_64/cpuid.h>
#include <arch/x86_64/pit.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <drivers/e1000.h>
#include <lib/printf.h>
#include <lib/string.h>

static vfs_node_t* procfs_root = NULL;

static ssize_t proc_version_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    (void)node;
    char text[256];
    int len = sprintf(text, "SUB-OS version 0.2.0-lts (subhobhai@sub-os) (x86_64-elf-gcc 13.3.0) #1 SMP PREEMPT Sun Aug 16 2026\n");
    if (offset >= len) return 0;
    if (offset + size > (size_t)len) size = len - offset;
    memcpy(buffer, text + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_uptime_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    (void)node;
    uint64_t ticks = pit_get_ticks();
    uint64_t secs = ticks / 100;
    uint64_t frac = (ticks % 100);

    char text[64];
    int len = sprintf(text, "%llu.%02llu %llu.%02llu\n", secs, frac, secs, frac);
    if (offset >= len) return 0;
    if (offset + size > (size_t)len) size = len - offset;
    memcpy(buffer, text + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_meminfo_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    (void)node;
    uint64_t total_kb = (pmm_get_total_pages() * 4096) / 1024;
    uint64_t free_kb  = (pmm_get_free_pages() * 4096) / 1024;
    uint64_t used_kb  = (pmm_get_used_pages() * 4096) / 1024;

    char text[512];
    int len = sprintf(text,
        "MemTotal:       %8llu kB\n"
        "MemFree:        %8llu kB\n"
        "MemAvailable:   %8llu kB\n"
        "Buffers:               0 kB\n"
        "Cached:         %8llu kB\n"
        "HeapTotal:      %8llu kB\n"
        "HeapUsed:       %8llu kB\n"
        "HeapFree:       %8llu kB\n",
        total_kb, free_kb, free_kb,
        (heap_get_used_bytes() / 1024),
        heap_get_total_bytes() / 1024,
        heap_get_used_bytes() / 1024,
        heap_get_free_bytes() / 1024);

    if (offset >= len) return 0;
    if (offset + size > (size_t)len) size = len - offset;
    memcpy(buffer, text + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_cpuinfo_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    (void)node;
    const cpu_info_t* cpu = cpuid_get_info();

    char text[1024];
    int len = sprintf(text,
        "processor\t: 0\n"
        "vendor_id\t: %s\n"
        "cpu family\t: %u\n"
        "model\t\t: %u\n"
        "model name\t: %s\n"
        "stepping\t: %u\n"
        "flags\t\t: %s%s%s%s%s%s%s%s%s\n"
        "bogomips\t: 4800.00\n"
        "clflush size\t: 64\n",
        cpu->vendor,
        cpu->family,
        cpu->model_id,
        cpu->model,
        cpu->stepping,
        cpu->has_fpu ? "fpu " : "",
        cpu->has_tsc ? "tsc " : "",
        cpu->has_pae ? "pae " : "",
        cpu->has_apic ? "apic " : "",
        cpu->has_sse ? "sse " : "",
        cpu->has_sse2 ? "sse2 " : "",
        cpu->has_sse3 ? "sse3 " : "",
        cpu->has_sse4_1 ? "sse4_1 " : "",
        cpu->has_avx ? "avx " : "");

    if (offset >= len) return 0;
    if (offset + size > (size_t)len) size = len - offset;
    memcpy(buffer, text + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_net_dev_read(vfs_node_t* node, off_t offset, size_t size, uint8_t* buffer) {
    (void)node;
    char text[512];
    int len = sprintf(text,
        "Inter-|   Receive                                                |  Transmit\n"
        " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
        "    lo:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0\n"
        "  eth0: %7llu %7llu    0    0    0     0          0         0  %7llu %7llu    0    0    0     0       0          0\n",
        e1000_get_rx_bytes(), e1000_get_rx_packets(),
        e1000_get_tx_bytes(), e1000_get_tx_packets());

    if (offset >= len) return 0;
    if (offset + size > (size_t)len) size = len - offset;
    memcpy(buffer, text + offset, size);
    return (ssize_t)size;
}

void procfs_init(void) {
    procfs_root = ramfs_add_dir(NULL, "proc");

    vfs_node_t* ver = ramfs_add_file(procfs_root, "version", NULL, 0);
    if (ver) { ver->flags = FS_FILE; ver->read = proc_version_read; }

    vfs_node_t* uptime = ramfs_add_file(procfs_root, "uptime", NULL, 0);
    if (uptime) { uptime->flags = FS_FILE; uptime->read = proc_uptime_read; }

    vfs_node_t* mem = ramfs_add_file(procfs_root, "meminfo", NULL, 0);
    if (mem) { mem->flags = FS_FILE; mem->read = proc_meminfo_read; }

    vfs_node_t* cpu = ramfs_add_file(procfs_root, "cpuinfo", NULL, 0);
    if (cpu) { cpu->flags = FS_FILE; cpu->read = proc_cpuinfo_read; }

    vfs_node_t* net_dir = ramfs_add_dir(procfs_root, "net");
    if (net_dir) {
        vfs_node_t* net_dev = ramfs_add_file(net_dir, "dev", NULL, 0);
        if (net_dev) { net_dev->flags = FS_FILE; net_dev->read = proc_net_dev_read; }
    }
}

vfs_node_t* procfs_get_root(void) {
    return procfs_root;
}
