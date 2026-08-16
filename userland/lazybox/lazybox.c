#include <userland/lazybox.h>
#include <fs/vfs.h>
#include <net/net.h>
#include <drivers/e1000.h>
#include <drivers/ata.h>
#include <drivers/tty.h>
#include <crypto/crypto.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/cpuid.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/printf.h>

// -------------------------------------------------------------
// Core Utilities
// -------------------------------------------------------------

static int applet_ls(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] : "/";
    vfs_node_t* dir = vfs_namei(path);

    if (!dir) {
        printk(KERN_ERR "ls: cannot access '%s': No such file or directory\n", path);
        return 1;
    }

    if (!(dir->flags & FS_DIRECTORY)) {
        printk(KERN_INFO "%s\n", path);
        return 0;
    }

    printk(ANSI_BRIGHT_CYAN "Type       Size (B)  Inode   Name\n" ANSI_RESET);
    printk("-----------------------------------------\n");

    uint32_t idx = 0;
    struct vfs_dirent* ent;
    while ((ent = dir->readdir(dir, idx++)) != NULL) {
        const char* color = ANSI_WHITE;
        const char* type_str = "FILE ";
        if (ent->type & FS_DIRECTORY) {
            color = ANSI_BRIGHT_BLUE;
            type_str = "DIR  ";
        } else if (ent->type & FS_CHARDEVICE) {
            color = ANSI_BRIGHT_YELLOW;
            type_str = "CHR  ";
        }

        vfs_node_t* child = dir->finddir ? dir->finddir(dir, ent->name) : NULL;
        size_t sz = child ? child->length : 0;

        printk("%s  %8llu  %6llu  %s%s" ANSI_RESET "\n",
               type_str, sz, ent->inode, color, ent->name);
    }
    return 0;
}

static int applet_cat(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: cat <file>\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        int fd = vfs_open(argv[i], O_RDONLY);
        if (fd < 0) {
            printk(KERN_ERR "cat: %s: No such file or directory\n", argv[i]);
            continue;
        }

        char buf[512];
        ssize_t bytes;
        while ((bytes = vfs_read(fd, buf, sizeof(buf) - 1)) > 0) {
            buf[bytes] = '\0';
            printk("%s", buf);
        }
        vfs_close(fd);
    }
    return 0;
}

static int applet_touch(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: touch <file...>\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        int fd = vfs_open(argv[i], O_CREAT | O_RDWR);
        if (fd >= 0) {
            vfs_close(fd);
        }
    }
    return 0;
}

static int applet_mkdir(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: mkdir <directory>\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (vfs_mkdir(argv[i], 0755) != 0) {
            printk(KERN_ERR "mkdir: cannot create directory '%s'\n", argv[i]);
        }
    }
    return 0;
}

static int applet_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        printk("%s%s", argv[i], (i == argc - 1) ? "" : " ");
    }
    printk("\n");
    return 0;
}

static int applet_wc(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: wc <file>\n");
        return 1;
    }

    int fd = vfs_open(argv[1], O_RDONLY);
    if (fd < 0) {
        printk(KERN_ERR "wc: %s: No such file\n", argv[1]);
        return 1;
    }

    int lines = 0, words = 0, bytes = 0;
    bool in_word = false;
    char buf[256];
    ssize_t n;

    while ((n = vfs_read(fd, buf, sizeof(buf))) > 0) {
        bytes += (int)n;
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') lines++;
            if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\r') {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                words++;
            }
        }
    }
    vfs_close(fd);

    printk(KERN_INFO "%4d %4d %4d %s\n", lines, words, bytes, argv[1]);
    return 0;
}

// -------------------------------------------------------------
// Cryptographic Hash Utilities
// -------------------------------------------------------------

static int applet_md5sum(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: md5sum <file|text>\n");
        return 1;
    }

    int fd = vfs_open(argv[1], O_RDONLY);
    if (fd >= 0) {
        md5_ctx_t ctx;
        md5_init(&ctx);
        char buf[512];
        ssize_t n;
        while ((n = vfs_read(fd, buf, sizeof(buf))) > 0) {
            md5_update(&ctx, buf, (size_t)n);
        }
        vfs_close(fd);

        uint8_t hash[16];
        md5_final(hash, &ctx);
        for (int i = 0; i < 16; i++) printk("%02x", hash[i]);
        printk("  %s\n", argv[1]);
    } else {
        // String mode
        char out[33];
        md5_string(argv[1], out);
        printk("%s  \"%s\"\n", out, argv[1]);
    }
    return 0;
}

static int applet_sha256sum(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: sha256sum <file|text>\n");
        return 1;
    }

    int fd = vfs_open(argv[1], O_RDONLY);
    if (fd >= 0) {
        sha256_ctx_t ctx;
        sha256_init(&ctx);
        char buf[512];
        ssize_t n;
        while ((n = vfs_read(fd, buf, sizeof(buf))) > 0) {
            sha256_update(&ctx, buf, (size_t)n);
        }
        vfs_close(fd);

        uint8_t hash[32];
        sha256_final(hash, &ctx);
        for (int i = 0; i < 32; i++) printk("%02x", hash[i]);
        printk("  %s\n", argv[1]);
    } else {
        char out[65];
        sha256_string(argv[1], out);
        printk("%s  \"%s\"\n", out, argv[1]);
    }
    return 0;
}

static int applet_crc32(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: crc32 <text>\n");
        return 1;
    }

    uint32_t val = crc32(0, argv[1], strlen(argv[1]));
    printk("0x%08X  \"%s\"\n", val, argv[1]);
    return 0;
}

// -------------------------------------------------------------
// Network Utilities
// -------------------------------------------------------------

static int applet_ifconfig(int argc, char** argv) {
    (void)argc; (void)argv;
    net_if_t* net_if = net_get_primary_if();

    char ip_s[16], gw_s[16], mask_s[16], mac_s[18];
    ip_to_str(net_if->ip, ip_s);
    ip_to_str(net_if->gateway, gw_s);
    ip_to_str(net_if->subnet, mask_s);
    mac_to_str(net_if->mac, mac_s);

    printk(ANSI_BRIGHT_GREEN "%s: " ANSI_RESET "flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500\n", net_if->name);
    printk("        inet " ANSI_BRIGHT_YELLOW "%s" ANSI_RESET "  netmask %s  broadcast %s\n", ip_s, mask_s, gw_s);
    printk("        ether " ANSI_BRIGHT_CYAN "%s" ANSI_RESET "  txqueuelen 1000  (Ethernet)\n", mac_s);
    printk("        RX packets %llu  bytes %llu\n", e1000_get_rx_packets(), e1000_get_rx_bytes());
    printk("        TX packets %llu  bytes %llu\n", e1000_get_tx_packets(), e1000_get_tx_bytes());
    printk("        Link Status: %s\n", e1000_is_link_up() ? ANSI_GREEN "UP" ANSI_RESET : ANSI_RED "DOWN" ANSI_RESET);
    return 0;
}

static int applet_ping(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: ping <ip-address> [count]\n");
        return 1;
    }

    uint32_t target_ip = ip_parse(argv[1]);
    if (target_ip == 0) {
        printk(KERN_ERR "Invalid IP address: %s\n", argv[1]);
        return 1;
    }

    uint32_t count = 4;
    if (argc >= 3) {
        int val = atoi(argv[2]);
        if (val > 0) count = (uint32_t)val;
    }

    net_ping(target_ip, count, 1000);
    return 0;
}

static int applet_arp(int argc, char** argv) {
    (void)argc; (void)argv;
    int count = 0;
    arp_entry_t* tbl = net_get_arp_table(&count);

    printk(ANSI_BRIGHT_CYAN "Address          HWtype  HWaddress           Flags  Iface\n" ANSI_RESET);
    char ip_s[16], mac_s[18];
    for (int i = 0; i < count; i++) {
        if (tbl[i].valid) {
            ip_to_str(tbl[i].ip, ip_s);
            mac_to_str(tbl[i].mac, mac_s);
            printk("%-16s ether   %-18s  C      eth0\n", ip_s, mac_s);
        }
    }
    return 0;
}

// -------------------------------------------------------------
// System & Hardware Utilities
// -------------------------------------------------------------

static int applet_uname(int argc, char** argv) {
    bool all = (argc >= 2 && strcmp(argv[1], "-a") == 0);
    if (all) {
        printk("SUB-OS sub-node 0.2.0-lts #1 SMP PREEMPT Sun Aug 16 2026 x86_64 GNU/LazyBox\n");
    } else {
        printk("SUB-OS\n");
    }
    return 0;
}

static int applet_free(int argc, char** argv) {
    (void)argc; (void)argv;
    uint64_t total_kb = (pmm_get_total_pages() * 4096) / 1024;
    uint64_t used_kb  = (pmm_get_used_pages() * 4096) / 1024;
    uint64_t free_kb  = (pmm_get_free_pages() * 4096) / 1024;

    size_t heap_used = heap_get_used_bytes();
    size_t heap_free = heap_get_free_bytes();

    printk(ANSI_BRIGHT_CYAN "               total        used        free      shared  buff/cache   available\n" ANSI_RESET);
    printk("Mem:    %12llu%12llu%12llu           0%12llu%12llu\n",
           total_kb, used_kb, free_kb, (heap_used / 1024), free_kb);
    printk("Heap:   %12llu%12llu%12llu\n",
           (heap_used + heap_free) / 1024, heap_used / 1024, heap_free / 1024);
    return 0;
}

static int applet_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    uint64_t ticks = pit_get_ticks();
    uint64_t secs = ticks / 100;
    uint64_t mins = secs / 60;
    uint64_t hours = mins / 60;
    secs %= 60;
    mins %= 60;

    printk("up %02llu:%02llu:%02llu,  1 user,  load average: 0.00, 0.01, 0.00\n", hours, mins, secs);
    return 0;
}

static int applet_dmesg(int argc, char** argv) {
    (void)argc; (void)argv;
    dmesg_dump();
    return 0;
}

static int applet_hdparm(int argc, char** argv) {
    (void)argc; (void)argv;
    const ata_device_t* dev = ata_get_primary_master();
    if (!dev || !dev->present) {
        printk(KERN_ERR "No ATA Hard Disk Drive detected.\n");
        return 1;
    }

    printk(ANSI_BRIGHT_CYAN "/dev/sda (ATA Primary Master):\n" ANSI_RESET);
    printk("  Model Number:       %s\n", dev->model);
    printk("  Capacity:           %llu MB (%u sectors)\n",
           ((uint64_t)dev->sector_count * 512) / (1024 * 1024), dev->sector_count);
    printk("  Sector Size:        512 bytes logical/physical\n");
    printk("  Addressing:         28-bit LBA Mode\n");
    return 0;
}

static int applet_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    tty_clear();
    return 0;
}

static int applet_sleep(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: sleep <seconds>\n");
        return 1;
    }
    int secs = atoi(argv[1]);
    if (secs > 0) {
        pit_sleep((uint32_t)secs * 1000);
    }
    return 0;
}

static int applet_lazybox(int argc, char** argv);

// Applet Dispatch Table
static const lazybox_applet_t applets[] = {
    {"lazybox",   applet_lazybox,   "lazybox [applet]",          "Multi-call utility manager", "Core"},
    {"ls",        applet_ls,        "ls [path]",                 "List directory contents",    "Filesystem"},
    {"cat",       applet_cat,       "cat <file...>",             "Concatenate files to stdout","Filesystem"},
    {"touch",     applet_touch,     "touch <file...>",           "Create empty files",         "Filesystem"},
    {"mkdir",     applet_mkdir,     "mkdir <dir>",               "Create directory",           "Filesystem"},
    {"wc",        applet_wc,        "wc <file>",                 "Count lines, words, bytes",  "Filesystem"},
    {"echo",      applet_echo,      "echo [text...]",            "Display text to stdout",     "Core"},
    {"md5sum",    applet_md5sum,    "md5sum <file|text>",        "Compute MD5 hash",           "Crypto"},
    {"sha256sum", applet_sha256sum, "sha256sum <file|text>",     "Compute SHA-256 hash",       "Crypto"},
    {"crc32",     applet_crc32,     "crc32 <text>",              "Calculate CRC32 checksum",   "Crypto"},
    {"ifconfig",  applet_ifconfig,  "ifconfig",                  "Display network interface",  "Network"},
    {"ping",      applet_ping,      "ping <ip> [count]",         "ICMP Echo ping utility",     "Network"},
    {"arp",       applet_arp,       "arp",                       "View ARP cache table",       "Network"},
    {"hdparm",    applet_hdparm,    "hdparm",                    "Inspect ATA hard disk",      "Storage"},
    {"uname",     applet_uname,     "uname [-a]",                "Print system architecture",  "System"},
    {"free",      applet_free,      "free",                      "Display RAM and Heap usage", "System"},
    {"uptime",    applet_uptime,    "uptime",                    "System running duration",    "System"},
    {"dmesg",     applet_dmesg,     "dmesg",                     "Dump kernel boot ringbuffer","System"},
    {"sleep",     applet_sleep,     "sleep <seconds>",           "Delay execution",            "System"},
    {"clear",     applet_clear,     "clear",                     "Clear console screen",       "Terminal"},
    {NULL, NULL, NULL, NULL, NULL}
};

static int applet_lazybox(int argc, char** argv) {
    if (argc >= 2) {
        return lazybox_run_applet(argv[1], argc - 1, &argv[1]);
    }

    printk(ANSI_BRIGHT_CYAN "LazyBox v%s (2026-08-16) Linux-Compatible Multi-Call Suite\n" ANSI_RESET, LAZYBOX_VERSION);
    printk("Usage: lazybox [function] [arguments]...\n\n");
    printk(ANSI_YELLOW "Defined Applet Categories:\n" ANSI_RESET);

    const char* cats[] = {"Filesystem", "Crypto", "Network", "Storage", "System", "Terminal", "Core", NULL};
    for (int c = 0; cats[c] != NULL; c++) {
        printk(ANSI_BRIGHT_GREEN "  [%s]\n   " ANSI_RESET, cats[c]);
        for (int i = 0; applets[i].name != NULL; i++) {
            if (strcmp(applets[i].category, cats[c]) == 0) {
                printk(" %-12s", applets[i].name);
            }
        }
        printk("\n");
    }
    printk("\n");
    return 0;
}

void lazybox_init(void) {
    // Initialized
}

bool lazybox_has_applet(const char* name) {
    if (!name) return false;
    for (int i = 0; applets[i].name != NULL; i++) {
        if (strcmp(applets[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

int lazybox_run_applet(const char* name, int argc, char** argv) {
    if (!name) return -1;
    for (int i = 0; applets[i].name != NULL; i++) {
        if (strcmp(applets[i].name, name) == 0) {
            return applets[i].func(argc, argv);
        }
    }
    printk(KERN_ERR "lazybox: %s: applet not found\n", name);
    return 127;
}

const lazybox_applet_t* lazybox_get_applets(int* count_out) {
    int count = 0;
    while (applets[count].name != NULL) count++;
    if (count_out) *count_out = count;
    return applets;
}
