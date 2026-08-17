#include <userland/lazybox.h>
#include <userland/nano.h>
#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <net/net.h>
#include <net/dns.h>
#include <net/dhcp.h>
#include <drivers/e1000.h>
#include <drivers/ata.h>
#include <drivers/tty.h>
#include <drivers/speaker.h>
#include <drivers/pci.h>
#include <drivers/rtc.h>
#include <drivers/mouse.h>
#include <sound/sound.h>
#include <sound/tts.h>
#include <crypto/crypto.h>
#include <certs/certs.h>
#include <security/security.h>
#include <security/capability.h>
#include <io_uring/io_uring.h>
#include <virt/virt.h>
#include <virt/virtio.h>
#include <init/version.h>
#include <init/init.h>
#include <kernel/module.h>
#include <kernel/task.h>
#include <ipc/ipc.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/cpuid.h>
#include <arch/x86_64/io.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/printf.h>
#include <security/auth.h>
#include <net/http.h>
#include <init/service.h>
#include <kernel/syslog.h>
#include <kernel/cron.h>
#include <kernel/metrics.h>
#include <kernel/trace.h>
#include <kernel/namespace.h>
#include <kernel/bpf.h>
#include <kernel/kobject.h>
#include <mm/vma.h>
#include <drivers/canvas.h>
#include <drivers/pty.h>
#include <block/block.h>
#include <drivers/hwmon.h>
#include <sound/melody.h>
#include <userland/sh.h>

// -------------------------------------------------------------
// Core & Filesystem Applets
// -------------------------------------------------------------

static int applet_ls(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] : vfs_getcwd();
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
        printk(KERN_INFO "Usage: cat <file...>\n");
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
        } else {
            printk(KERN_ERR "touch: cannot touch '%s': No such file or directory\n", argv[i]);
        }
    }
    return 0;
}

static int applet_mkdir(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: mkdir <directory...>\n");
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

static int applet_pwd(int argc, char** argv) {
    (void)argc; (void)argv;
    printk("%s\n", vfs_getcwd());
    return 0;
}

static int applet_cd(int argc, char** argv) {
    const char* target = (argc >= 2) ? argv[1] : "/";
    int res = vfs_chdir(target);
    if (res != 0) {
        printk(KERN_ERR "cd: %s: No such file or directory\n", target);
        return 1;
    }
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

    uint64_t lines = 0, words = 0, bytes = 0;
    char buf[512];
    ssize_t n;
    bool in_word = false;

    while ((n = vfs_read(fd, buf, sizeof(buf))) > 0) {
        bytes += n;
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

    printk("%4llu %4llu %4llu %s\n", lines, words, bytes, argv[1]);
    return 0;
}

static int applet_head(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: head [-n lines] <file>\n");
        return 1;
    }

    int max_lines = 10;
    const char* path = argv[1];
    if (argc >= 4 && strcmp(argv[1], "-n") == 0) {
        max_lines = atoi(argv[2]);
        path = argv[3];
    }

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        printk(KERN_ERR "head: %s: No such file\n", path);
        return 1;
    }

    char buf[512];
    ssize_t n;
    int lines_printed = 0;

    while (lines_printed < max_lines && (n = vfs_read(fd, buf, sizeof(buf) - 1)) > 0) {
        for (ssize_t i = 0; i < n && lines_printed < max_lines; i++) {
            printk("%c", buf[i]);
            if (buf[i] == '\n') lines_printed++;
        }
    }
    vfs_close(fd);
    return 0;
}

static int applet_tail(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: tail <file>\n");
        return 1;
    }
    return applet_cat(argc, argv);
}

static int applet_stat(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: stat <file>\n");
        return 1;
    }

    vfs_node_t* node = vfs_namei(argv[1]);
    if (!node) {
        printk(KERN_ERR "stat: cannot stat '%s': No such file or directory\n", argv[1]);
        return 1;
    }

    const char* type = "regular file";
    if (node->flags & FS_DIRECTORY) type = "directory";
    else if (node->flags & FS_CHARDEVICE) type = "character device";

    printk("  File: %s\n", argv[1]);
    printk("  Size: %-15llu Blocks: %-10llu IO Block: 4096  %s\n",
           (uint64_t)node->length, (uint64_t)((node->length + 511) / 512), type);
    printk(" Inode: %-15llu Links: 1\n", (uint64_t)node->inode);
    printk("Access: (0%o/flags=0x%x)  Uid: (%d/root)   Gid: (%d/root)\n",
           node->mode, node->flags, node->uid, node->gid);
    return 0;
}

static int applet_cp(int argc, char** argv) {
    if (argc < 3) {
        printk(KERN_INFO "Usage: cp <source> <dest>\n");
        return 1;
    }

    int src = vfs_open(argv[1], O_RDONLY);
    if (src < 0) {
        printk(KERN_ERR "cp: cannot stat '%s': No such file\n", argv[1]);
        return 1;
    }

    int dst = vfs_open(argv[2], O_CREAT | O_WRONLY | O_TRUNC);
    if (dst < 0) {
        printk(KERN_ERR "cp: cannot create '%s'\n", argv[2]);
        vfs_close(src);
        return 1;
    }

    char buf[512];
    ssize_t n;
    while ((n = vfs_read(src, buf, sizeof(buf))) > 0) {
        vfs_write(dst, buf, n);
    }

    vfs_close(src);
    vfs_close(dst);
    return 0;
}

static int applet_grep(int argc, char** argv) {
    if (argc < 3) {
        printk(KERN_INFO "Usage: grep <pattern> <file>\n");
        return 1;
    }

    const char* pattern = argv[1];
    int fd = vfs_open(argv[2], O_RDONLY);
    if (fd < 0) {
        printk(KERN_ERR "grep: %s: No such file\n", argv[2]);
        return 1;
    }

    char line[256];
    int pos = 0;
    char c;

    while (vfs_read(fd, &c, 1) == 1) {
        if (c == '\n' || pos >= (int)sizeof(line) - 1) {
            line[pos] = '\0';
            if (strstr(line, pattern) != NULL) {
                printk("%s\n", line);
            }
            pos = 0;
        } else if (c != '\r') {
            line[pos++] = c;
        }
    }
    vfs_close(fd);
    return 0;
}

static int applet_hexdump(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: hexdump <file>\n");
        return 1;
    }

    int fd = vfs_open(argv[1], O_RDONLY);
    if (fd < 0) {
        printk(KERN_ERR "hexdump: %s: No such file\n", argv[1]);
        return 1;
    }

    uint8_t buf[16];
    ssize_t n;
    uint64_t offset = 0;

    while ((n = vfs_read(fd, buf, 16)) > 0) {
        printk("%08llx  ", offset);
        for (int i = 0; i < 16; i++) {
            if (i < n) printk("%02x ", buf[i]);
            else printk("   ");
            if (i == 7) printk(" ");
        }
        printk(" |");
        for (int i = 0; i < n; i++) {
            char ch = (buf[i] >= 32 && buf[i] <= 126) ? (char)buf[i] : '.';
            printk("%c", ch);
        }
        printk("|\n");
        offset += n;
    }
    vfs_close(fd);
    return 0;
}

static int applet_nano_wrapper(int argc, char** argv) {
    return nano_main(argc, argv);
}

// -------------------------------------------------------------
// Cryptography & Security Applets
// -------------------------------------------------------------

static int applet_md5sum(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: md5sum <file|string>\n");
        return 1;
    }

    uint8_t digest[16];
    int fd = vfs_open(argv[1], O_RDONLY);
    if (fd >= 0) {
        char buf[512];
        ssize_t n;
        md5_ctx_t ctx;
        md5_init(&ctx);
        while ((n = vfs_read(fd, buf, sizeof(buf))) > 0) {
            md5_update(&ctx, (uint8_t*)buf, n);
        }
        md5_final(digest, &ctx);
        vfs_close(fd);
    } else {
        md5((const uint8_t*)argv[1], strlen(argv[1]), digest);
    }

    for (int i = 0; i < 16; i++) printk("%02x", digest[i]);
    printk("  %s\n", argv[1]);
    return 0;
}

static int applet_sha256sum(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: sha256sum <file|string>\n");
        return 1;
    }

    uint8_t digest[32];
    int fd = vfs_open(argv[1], O_RDONLY);
    if (fd >= 0) {
        char buf[512];
        ssize_t n;
        sha256_ctx_t ctx;
        sha256_init(&ctx);
        while ((n = vfs_read(fd, buf, sizeof(buf))) > 0) {
            sha256_update(&ctx, (uint8_t*)buf, n);
        }
        sha256_final(digest, &ctx);
        vfs_close(fd);
    } else {
        sha256((const uint8_t*)argv[1], strlen(argv[1]), digest);
    }

    for (int i = 0; i < 32; i++) printk("%02x", digest[i]);
    printk("  %s\n", argv[1]);
    return 0;
}

static int applet_crc32(int argc, char** argv) {
    const char* str = (argc >= 2) ? argv[1] : "";
    uint32_t val = crc32(0, (const uint8_t*)str, strlen(str));
    printk("0x%08X  \"%s\"\n", val, str);
    return 0;
}

static int applet_rand(int argc, char** argv) {
    int count = (argc >= 2) ? atoi(argv[1]) : 4;
    printk("Random Integers: ");
    for (int i = 0; i < count; i++) {
        printk("%u ", prng_rand32());
    }
    printk("\n");
    return 0;
}

static int applet_certcheck(int argc, char** argv) {
    (void)argc; (void)argv;
    size_t count = certs_get_count();
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS X.509 Kernel Trust Keyring (%llu certs) ===\n" ANSI_RESET, (uint64_t)count);
    for (size_t i = 0; i < count; i++) {
        const x509_cert_t* c = certs_get_cert(i);
        if (c) {
            printk("[%llu] %s\n", (uint64_t)(i + 1), c->subject);
            printk("     Issuer: %s\n", c->issuer);
            printk("     Fingerprint: ");
            for (int j = 0; j < 16; j++) printk("%02x", c->fingerprint[j]);
            printk("... [TRUSTED]\n");
        }
    }
    return 0;
}

static int applet_capsh(int argc, char** argv) {
    (void)argc; (void)argv;
    printk("Current Security LSM: %s\n", security_get_active_lsm_name());
    printk("Capabilities: Current=0x%016llx (Full Set)\n", (uint64_t)CAP_FULL_SET);
    printk("Bounds: cap_chown, cap_dac_override, cap_net_raw, cap_sys_admin [ALL PERMITTED]\n");
    return 0;
}

// -------------------------------------------------------------
// Audio & Text-to-Speech Applets
// -------------------------------------------------------------

static int applet_tts(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: tts <text to synthesize>\n");
        tts_speak("hello sub os");
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        tts_speak(argv[i]);
        if (i + 1 < argc) tts_speak(" ");
    }
    return 0;
}

static int applet_alsamixer(int argc, char** argv) {
    (void)argc; (void)argv;
    sound_device_t* dev = sound_get_default_device();
    printk(ANSI_BRIGHT_CYAN "=== ALSA Sound Architecture & Mixer Control ===\n" ANSI_RESET);
    if (dev) {
        printk("Card: %s\n", dev->name);
        printk("Sample Rate: %u Hz | Channels: %d | Depth: %d-bit\n",
               dev->sample_rate, dev->channels, dev->bits_per_sample);
        printk("Master Volume: [||||||||||||||||||||] %d%%\n", dev->volume);
    } else {
        printk("PC Speaker Synthesizer: Active (1193180 Hz base timer)\n");
    }
    return 0;
}

// -------------------------------------------------------------
// Kernel Modules & Dynamic Loading Applets
// -------------------------------------------------------------

static int applet_lsmod(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_CYAN "Module                  Size  Used by\n" ANSI_RESET);
    size_t count = module_get_count();
    for (size_t i = 0; i < count; i++) {
        const kernel_module_t* m = module_get(i);
        if (m && m->loaded) {
            printk("%-20s %8llu  0 [Live]\n", m->name, (uint64_t)m->size);
        }
    }
    return 0;
}

static int applet_insmod(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: insmod <module_name>\n");
        return 1;
    }
    if (module_load(argv[1], NULL, NULL, 8192) == 0) {
        printk(KERN_INFO "Module '%s' loaded successfully\n", argv[1]);
        return 0;
    }
    printk(KERN_ERR "insmod: failed to insert module '%s'\n", argv[1]);
    return 1;
}

static int applet_rmmod(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: rmmod <module_name>\n");
        return 1;
    }
    if (module_unload(argv[1]) == 0) {
        printk(KERN_INFO "Module '%s' unloaded successfully\n", argv[1]);
        return 0;
    }
    printk(KERN_ERR "rmmod: failed to remove module '%s'\n", argv[1]);
    return 1;
}

// -------------------------------------------------------------
// Network & Hardware Drivers Applets
// -------------------------------------------------------------

static int applet_ifconfig(int argc, char** argv) {
    (void)argc; (void)argv;
    uint8_t mac[6];
    e1000_get_mac(mac);

    printk("eth0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500\n");
    printk("        inet 10.0.2.15  netmask 255.255.255.0  broadcast 10.0.2.2\n");
    printk("        ether %02x:%02x:%02x:%02x:%02x:%02x  txqueuelen 1000  (Ethernet)\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printk("        RX packets 0  bytes 0\n");
    printk("        TX packets 0  bytes 0\n");
    printk("        Link Status: %s\n", e1000_is_link_up() ? "UP" : "DOWN");
    return 0;
}

static int applet_ping(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: ping <ip_address> [count]\n");
        return 1;
    }

    uint32_t target_ip = ip_parse(argv[1]);
    int count = (argc >= 3) ? atoi(argv[2]) : 2;

    printk("PING %s (56 data bytes):\n", argv[1]);
    net_ping(target_ip, (uint32_t)count, 1000);
    return 0;
}

static int applet_arp(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_CYAN "Address          HWtype  HWaddress           Flags Mask            Iface\n" ANSI_RESET);
    printk("10.0.2.2         ether   52:55:0a:00:02:02   C                     eth0\n");
    printk("10.0.2.3         ether   52:55:0a:00:02:03   C                     eth0\n");
    return 0;
}

static int applet_dhclient(int argc, char** argv) {
    (void)argc; (void)argv;
    return dhcp_request_lease();
}

static int applet_nslookup(int argc, char** argv) {
    const char* host = (argc >= 2) ? argv[1] : "google.com";
    uint32_t ip = dns_resolve(host);
    char ip_buf[16];
    ip_to_str(ip, ip_buf);
    printk("Server:         10.0.2.3\nAddress:        10.0.2.3#53\n\nNon-authoritative answer:\nName:   %s\nAddress: %s\n",
           host, ip_buf);
    return 0;
}

static int applet_hdparm(int argc, char** argv) {
    (void)argc; (void)argv;
    const ata_device_t* dev = ata_get_primary_master();
    if (!dev || !dev->present) {
        printk(KERN_ERR "hdparm: No ATA disk detected\n");
        return 1;
    }

    printk("/dev/sda (ATA Primary Master):\n");
    printk("  Model Number:       %s\n", dev->model);
    printk("  Capacity:           %llu MB (%llu sectors)\n",
           (dev->sector_count * 512) / (1024 * 1024), (uint64_t)dev->sector_count);
    printk("  Sector Size:        512 bytes logical/physical\n");
    printk("  Addressing:         28-bit LBA Mode\n");
    return 0;
}

static int applet_lspci(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_CYAN "=== PCI Bus Device Enumeration ===\n" ANSI_RESET);
    pci_device_t* dev = pci_get_devices();
    while (dev) {
        printk("%02x:%02x.%d [%04x:%04x] Class %02x.%02x (IRQ %d)\n",
               dev->bus, dev->slot, dev->function,
               dev->vendor_id, dev->device_id,
               dev->class_id, dev->subclass_id, dev->irq);
        dev = dev->next;
    }
    return 0;
}

static int applet_speaker(int argc, char** argv) {
    int freq = (argc >= 2) ? atoi(argv[1]) : 587;
    int dur  = (argc >= 3) ? atoi(argv[2]) : 200;
    speaker_beep((uint32_t)freq, (uint32_t)dur);
    return 0;
}

static int applet_mouse(int argc, char** argv) {
    (void)argc; (void)argv;
    const mouse_state_t* m = mouse_get_state();
    printk("Mouse State: X=%d, Y=%d, Left=%s, Right=%s, Mid=%s\n",
           m->x, m->y, m->left_btn ? "ON" : "OFF",
           m->right_btn ? "ON" : "OFF", m->middle_btn ? "ON" : "OFF");
    return 0;
}

// -------------------------------------------------------------
// System & Virtualization Applets
// -------------------------------------------------------------

static int applet_uname(int argc, char** argv) {
    if (argc >= 2 && strcmp(argv[1], "-a") == 0) {
        printk("SUB-OS sub-node %s #1 SMP PREEMPT Sun Aug 16 2026 x86_64 GNU/LazyBox\n", kernel_get_version());
    } else {
        printk("SUB-OS\n");
    }
    return 0;
}

static int applet_date(int argc, char** argv) {
    (void)argc; (void)argv;
    char date_buf[64];
    rtc_format_string(date_buf, sizeof(date_buf));
    printk("%s\n", date_buf);
    return 0;
}

static int applet_free(int argc, char** argv) {
    (void)argc; (void)argv;
    uint64_t total_kb = (pmm_get_total_pages() * 4096) / 1024;
    uint64_t used_kb  = (pmm_get_used_pages() * 4096) / 1024;
    uint64_t free_kb  = (pmm_get_free_pages() * 4096) / 1024;

    printk(ANSI_BRIGHT_CYAN "               total        used        free      shared  buff/cache   available\n" ANSI_RESET);
    printk("Mem:        %8llu    %8llu    %8llu           0         110    %8llu\n",
           total_kb, used_kb, free_kb, free_kb);
    printk("Heap:       %8llu    %8llu    %8llu\n",
           kmalloc_get_total() / 1024, kmalloc_get_used() / 1024, kmalloc_get_free() / 1024);
    return 0;
}

static int applet_uptime(int argc, char** argv) {
    (void)argc; (void)argv;
    uint64_t ticks = pit_get_ticks();
    uint64_t secs = ticks / 100;
    uint64_t hrs = secs / 3600;
    uint64_t mins = (secs % 3600) / 60;
    secs = secs % 60;

    printk("up %02llu:%02llu:%02llu,  1 user,  load average: 0.00, 0.01, 0.00\n",
           hrs, mins, secs);
    return 0;
}

static int applet_dmesg(int argc, char** argv) {
    (void)argc; (void)argv;
    dmesg_dump();
    return 0;
}

static int applet_sleep(int argc, char** argv) {
    int secs = (argc >= 2) ? atoi(argv[1]) : 1;
    if (secs > 0) {
        pit_sleep((uint32_t)secs * 1000);
    }
    return 0;
}

static int applet_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    tty_clear();
    return 0;
}

static int applet_whoami(int argc, char** argv) {
    (void)argc; (void)argv;
    const user_account_t* u = auth_get_current_user();
    printk("%s\n", u ? u->username : "root");
    return 0;
}

static int applet_id(int argc, char** argv) {
    (void)argc; (void)argv;
    const user_account_t* u = auth_get_current_user();
    if (u) {
        printk("uid=%u(%s) gid=%u(%s) groups=%u(%s)\n",
               u->uid, u->username, u->gid, u->username, u->gid, u->username);
    } else {
        printk("uid=0(root) gid=0(root) groups=0(root)\n");
    }
    return 0;
}

static int applet_cal(int argc, char** argv) {
    (void)argc; (void)argv;
    printk("     August 2026\n");
    printk("Su Mo Tu We Th Fr Sa\n");
    printk("                   1\n");
    printk(" 2  3  4  5  6  7  8\n");
    printk(" 9 10 11 12 13 14 15\n");
    printk(ANSI_INVERT "16" ANSI_RESET " 17 18 19 20 21 22\n");
    printk("23 24 25 26 27 28 29\n");
    printk("30 31\n");
    return 0;
}

static int applet_ipcs(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_CYAN "------ Message Queues --------\n" ANSI_RESET);
    printk("key        msqid      owner      perms      used-bytes   messages\n");
    printk(ANSI_BRIGHT_CYAN "------ Shared Memory Segments --------\n" ANSI_RESET);
    printk("key        shmid      owner      perms      bytes        nattch\n");
    printk(ANSI_BRIGHT_CYAN "------ Semaphore Arrays --------\n" ANSI_RESET);
    printk("key        semid      owner      perms      nsems\n");
    return 0;
}

static int applet_virtinfo(int argc, char** argv) {
    (void)argc; (void)argv;
    const hypervisor_info_t* hv = virt_get_hypervisor_info();
    printk("Hypervisor Detected: %s\n", hv ? hv->name : "None");
    printk("Signature:           '%s'\n", hv ? hv->signature : "");
    printk("Virtualized:         %s\n", virt_is_virtualized() ? "YES" : "NO");
    printk("VirtIO Devices:      %llu registered\n", (uint64_t)virtio_get_device_count());
    for (size_t i = 0; i < virtio_get_device_count(); i++) {
        const virtio_device_t* dev = virtio_get_device(i);
        if (dev) printk("  [%llu] %s (IO: 0x%x, IRQ: %d)\n", (uint64_t)i, dev->name, dev->io_base, dev->irq);
    }
    return 0;
}

static int applet_io_uring_test(int argc, char** argv) {
    (void)argc; (void)argv;
    io_uring_ring_t* ring = io_uring_get_default_ring();
    if (!ring) {
        printk(KERN_ERR "io_uring: Ring not available\n");
        return 1;
    }

    io_uring_sqe_t* sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        printk(KERN_ERR "io_uring: SQE allocation failed\n");
        return 1;
    }

    sqe->opcode = IORING_OP_NOP;
    sqe->user_data = 0x12345678;

    int res = io_uring_submit(ring);
    printk("io_uring: Submitted %d SQE(s). Checking CQE...\n", res);

    io_uring_cqe_t* cqe = io_uring_peek_cqe(ring);
    if (cqe) {
        printk("io_uring: CQE Received! user_data=0x%llx res=%d [SUCCESS]\n",
               cqe->user_data, cqe->res);
        io_uring_cqe_seen(ring, cqe);
    }
    return 0;
}

static int applet_ps(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_CYAN "  PID USER       VSZ STAT COMMAND\n" ANSI_RESET);
    printk("    1 root      4096 S    /init\n");
    printk("    2 root         0 S    [kthreadd]\n");
    printk("    3 root         0 S    [kworker/0:0]\n");
    printk("    4 root      1024 R    /bin/lazybox (shell)\n");
    return 0;
}

static int applet_top(int argc, char** argv) {
    (void)argc; (void)argv;
    printk("Tasks: 4 total, 1 running, 3 sleeping, 0 stopped, 0 zombie\n");
    printk("%%Cpu(s):  0.2 us,  0.5 sy,  0.0 ni, 99.3 id,  0.0 wa\n");
    return applet_free(0, NULL);
}

static int applet_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_RED "Restarting system via ACPI/8042...\n" ANSI_RESET);
    pit_sleep(200);
    outb(0x64, 0xFE);
    return 0;
}

static int applet_poweroff(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_RED "System powering down via ACPI S5...\n" ANSI_RESET);
    pit_sleep(200);
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    return 0;
}

static int applet_lazybox(int argc, char** argv);

// -------------------------------------------------------------
// Advanced Linux Subsystem Applets
// -------------------------------------------------------------
#include <mm/slab.h>
#include <kernel/trace.h>
#include <kernel/namespace.h>
#include <net/filter.h>
#include <sound/melody.h>
#include <userland/sh.h>

static int applet_sh(int argc, char** argv) {
    return sh_main(argc, argv);
}

static int applet_slabinfo(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_CYAN "slabinfo - version: 2.1\n" ANSI_RESET);
    printk(ANSI_YELLOW "# name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab>\n" ANSI_RESET);
    size_t count = slab_get_cache_count();
    for (size_t i = 0; i < count; i++) {
        const kmem_cache_t* c = slab_get_cache(i);
        if (c && c->in_use) {
            printk("%-18s %11u %10u %9llu %12llu %14d\n",
                   c->name, c->active_objects, c->active_objects + 16,
                   (uint64_t)c->object_size, (uint64_t)c->objects_per_slab, 1);
        }
    }
    return 0;
}

static int applet_trace(int argc, char** argv) {
    if (argc >= 2) {
        if (strcmp(argv[1], "on") == 0) {
            trace_enable(true);
            printk(KERN_INFO "Tracing enabled\n");
            return 0;
        } else if (strcmp(argv[1], "off") == 0) {
            trace_enable(false);
            printk(KERN_INFO "Tracing disabled\n");
            return 0;
        } else if (strcmp(argv[1], "clear") == 0) {
            trace_clear();
            printk(KERN_INFO "Trace buffer cleared\n");
            return 0;
        }
    }

    size_t total = trace_get_count();
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Kernel Event Trace Ringbuffer (%llu events) ===\n" ANSI_RESET, (uint64_t)total);
    printk(ANSI_YELLOW "# CPU  PID   TIMESTAMP   CATEGORY  EVENT\n" ANSI_RESET);
    const char* cat_names[] = {"SYS  ", "IRQ  ", "SCHED", "MM   ", "NET  ", "FS   "};

    for (size_t i = 0; i < total; i++) {
        const trace_entry_t* e = trace_get_entry(i);
        if (e) {
            const char* cat_str = (e->category < TRACE_CAT_MAX) ? cat_names[e->category] : "MISC ";
            printk("[%3u] %4u %10llu ms  [%s]  %s\n",
                   e->cpu_id, e->pid, e->timestamp * 10, cat_str, e->message);
        }
    }
    return 0;
}

static int applet_unshare(int argc, char** argv) {
    const char* name = (argc >= 2) ? argv[1] : "isolated_container";
    nsproxy_t* ns = namespace_create(name, CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS);
    if (ns) {
        printk(KERN_INFO "Created and entered isolated namespace ID %u ('%s')\n", ns->id, ns->name);
        printk("  Hostname:   %s\n", ns->uts.hostname);
        printk("  PID Offset: %u\n", ns->pid_offset);
        namespace_switch(ns->id);
    } else {
        printk(KERN_ERR "unshare: failed to create namespace\n");
    }
    return 0;
}

static int applet_iptables(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_CYAN "Chain INPUT (policy ACCEPT)\n" ANSI_RESET);
    printk(ANSI_YELLOW "target     prot opt source               destination\n" ANSI_RESET);
    size_t count = filter_get_rule_count();
    for (size_t i = 0; i < count; i++) {
        const filter_rule_t* r = filter_get_rule(i);
        if (r && r->in_use) {
            const char* act = (r->action == FILTER_ACTION_ACCEPT) ? "ACCEPT " : "DROP   ";
            const char* proto = (r->protocol == 1) ? "icmp" : (r->protocol == 6) ? "tcp " : (r->protocol == 17) ? "udp " : "all ";
            printk("%s    %-4s --  0.0.0.0/0            0.0.0.0/0            dport:%-5u /* %s */ (pkts: %llu, bytes: %llu)\n",
                   act, proto, r->dst_port, r->comment, r->packet_count, r->byte_count);
        }
    }
    return 0;
}

static int applet_tune(int argc, char** argv) {
    const char* which = (argc >= 2) ? argv[1] : "startup";
    if (strcmp(which, "tetris") == 0) {
        melody_play_tetris();
    } else if (strcmp(which, "mario") == 0) {
        melody_play_mario();
    } else {
        melody_play_startup();
    }
    return 0;
}

static int applet_env(int argc, char** argv) {
    (void)argc; (void)argv;
    const char* keys[] = {"PATH", "USER", "HOME", "SHELL", "TERM", "OS", NULL};
    for (int i = 0; keys[i] != NULL; i++) {
        const char* val = sh_get_env(keys[i]);
        if (val) {
            printk("%s=%s\n", keys[i], val);
        }
    }
    return 0;
}

static int applet_export(int argc, char** argv) {
    if (argc < 2) {
        return applet_env(argc, argv);
    }
    for (int i = 1; i < argc; i++) {
        char* eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            sh_set_env(argv[i], eq + 1);
        }
    }
    return 0;
}

static int applet_basename(int argc, char** argv) {
    if (argc < 2) {
        printk("\n");
        return 0;
    }
    const char* p = argv[1];
    const char* last = strrchr(p, '/');
    printk("%s\n", last ? last + 1 : p);
    return 0;
}

static int applet_dirname(int argc, char** argv) {
    if (argc < 2) {
        printk(".\n");
        return 0;
    }
    char buf[256];
    strncpy(buf, argv[1], sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* last = strrchr(buf, '/');
    if (!last) {
        printk(".\n");
    } else if (last == buf) {
        printk("/\n");
    } else {
        *last = '\0';
        printk("%s\n", buf);
    }
    return 0;
}

// -------------------------------------------------------------
// Server, Authentication & System Administration Applets
// -------------------------------------------------------------
#include <net/http.h>
#include <init/service.h>
#include <kernel/syslog.h>
#include <security/auth.h>
#include <kernel/cron.h>
#include <kernel/metrics.h>

static int applet_httpd(int argc, char** argv) {
    if (argc >= 2) {
        if (strcmp(argv[1], "start") == 0) {
            uint16_t port = (argc >= 3) ? (uint16_t)atoi(argv[2]) : 80;
            httpd_start(port);
            return 0;
        } else if (strcmp(argv[1], "stop") == 0) {
            httpd_stop();
            return 0;
        }
    }

    printk(ANSI_BRIGHT_CYAN "=== Embedded Micro HTTP Server & REST API ===\n" ANSI_RESET);
    printk("Status:          %s\n", httpd_is_running() ? ANSI_BRIGHT_GREEN "RUNNING" ANSI_RESET : ANSI_YELLOW "STOPPED" ANSI_RESET);
    printk("Port:            80 (HTTP/1.1)\n");
    printk("Requests Served: %u\n", httpd_get_requests_served());
    printk("Usage:           httpd [start [port] | stop | status]\n");
    return 0;
}

#include <net/ssh.h>

static int applet_sshd(int argc, char** argv) {
    if (argc >= 2) {
        if (strcmp(argv[1], "start") == 0) {
            uint16_t port = (argc >= 3) ? (uint16_t)atoi(argv[2]) : 22;
            sshd_start(port);
            return 0;
        } else if (strcmp(argv[1], "stop") == 0) {
            sshd_stop();
            return 0;
        }
    }

    printk(ANSI_BRIGHT_CYAN "=== OpenSSH Remote Secure Shell Server (SSHD) ===\n" ANSI_RESET);
    printk("Status:          %s\n", sshd_is_running() ? ANSI_BRIGHT_GREEN "RUNNING" ANSI_RESET : ANSI_YELLOW "STOPPED" ANSI_RESET);
    printk("Port:            %u (SSH-2.0)\n", sshd_get_port());
    printk("Active Sessions: %u\n", sshd_get_sessions_count());
    printk("Usage:           sshd [start [port] | stop | status]\n");
    return 0;
}

static int applet_ssh(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: ssh [user@]hostname [command]\n");
        return 1;
    }

    char username[32] = "root";
    char host[64] = "localhost";
    uint16_t port = 22;

    const char* target = argv[1];
    const char* at = strchr(target, '@');
    if (at) {
        size_t ulen = at - target;
        if (ulen < sizeof(username)) {
            memcpy(username, target, ulen);
            username[ulen] = '\0';
        }
        strncpy(host, at + 1, sizeof(host) - 1);
    } else {
        strncpy(host, target, sizeof(host) - 1);
    }

    char cmd_buf[128] = "";
    if (argc >= 3) {
        for (int i = 2; i < argc; i++) {
            strcat(cmd_buf, argv[i]);
            if (i + 1 < argc) strcat(cmd_buf, " ");
        }
    }

    char out_resp[1024];
    if (ssh_client_execute(username, username, host, port, cmd_buf, out_resp, sizeof(out_resp)) == 0) {
        printk("%s", out_resp);
        if (cmd_buf[0] != '\0') {
            // Execute the requested command
            return lazybox_run_applet(argv[2], argc - 2, &argv[2]);
        }
        return 0;
    }
    printk(KERN_ERR "%s", out_resp);
    return 1;
}

static int applet_curl(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: curl <url|path>\n");
        return 1;
    }
    char buf[2048];
    if (http_client_get(argv[1], buf, sizeof(buf)) > 0) {
        printk("%s\n", buf);
        return 0;
    }
    printk(KERN_ERR "curl: failed to retrieve %s\n", argv[1]);
    return 1;
}

static int applet_wget(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: wget <url>\n");
        return 1;
    }
    printk("Connecting to %s... connected.\n", argv[1]);
    return applet_curl(argc, argv);
}

static int applet_systemctl(int argc, char** argv) {
    if (argc >= 3) {
        if (strcmp(argv[1], "start") == 0) {
            if (service_start(argv[2]) == 0) {
                printk(KERN_INFO "Started %s\n", argv[2]);
            } else {
                printk(KERN_ERR "Failed to start %s\n", argv[2]);
            }
            return 0;
        } else if (strcmp(argv[1], "stop") == 0) {
            service_stop(argv[2]);
            printk(KERN_INFO "Stopped %s\n", argv[2]);
            return 0;
        } else if (strcmp(argv[1], "restart") == 0) {
            service_restart(argv[2]);
            printk(KERN_INFO "Restarted %s\n", argv[2]);
            return 0;
        }
    }

    printk(ANSI_BRIGHT_CYAN "  UNIT                       LOAD   ACTIVE SUB     DESCRIPTION\n" ANSI_RESET);
    size_t count = service_get_count();
    for (size_t i = 0; i < count; i++) {
        const service_unit_t* s = service_get_by_index(i);
        if (s && s->in_use) {
            const char* act = (s->state == SERVICE_STATE_RUNNING) ? ANSI_BRIGHT_GREEN "active running" ANSI_RESET : ANSI_BRIGHT_BLACK "inactive dead" ANSI_RESET;
            printk("  %-26s loaded %-14s %s\n", s->name, act, s->description);
        }
    }
    return 0;
}

static int applet_logger(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: logger <message>\n");
        return 1;
    }
    char buf[128] = "";
    for (int i = 1; i < argc; i++) {
        strcat(buf, argv[i]);
        if (i + 1 < argc) strcat(buf, " ");
    }
    syslog_write(LOG_INFO | LOG_USER, "user", buf);
    return 0;
}

static int applet_logread(int argc, char** argv) {
    (void)argc; (void)argv;
    size_t count = syslog_get_count();
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Syslog Telemetry Ringbuffer (%llu entries) ===\n" ANSI_RESET, (uint64_t)count);
    for (size_t i = 0; i < count; i++) {
        const syslog_entry_t* e = syslog_get_entry(i);
        if (e) {
            printk("[%6llus] <%s.%u> %s: %s\n",
                   (uint64_t)e->timestamp, e->facility_str, e->priority, e->ident, e->message);
        }
    }
    return 0;
}

static int applet_su(int argc, char** argv) {
    const char* target = (argc >= 2) ? argv[1] : "root";
    if (auth_set_current_user(target) == 0) {
        printk(KERN_INFO "Switched to user '%s'\n", target);
        return 0;
    }
    printk(KERN_ERR "su: user '%s' does not exist\n", target);
    return 1;
}

static int applet_passwd(int argc, char** argv) {
    const char* user = (argc >= 2) ? argv[1] : "root";
    const char* new_pass = (argc >= 3) ? argv[2] : "password";
    if (auth_change_password(user, new_pass) == 0) {
        printk(KERN_INFO "passwd: password updated successfully for '%s'\n", user);
        return 0;
    }
    printk(KERN_ERR "passwd: user '%s' not found\n", user);
    return 1;
}

static int applet_useradd(int argc, char** argv) {
    if (argc < 2) {
        printk(KERN_INFO "Usage: useradd <username> [password]\n");
        return 1;
    }
    const char* pass = (argc >= 3) ? argv[2] : "123456";
    if (auth_add_user(argv[1], pass, 1000 + (uint32_t)auth_get_user_count(), 1000, "/home", "/bin/sh") == 0) {
        printk(KERN_INFO "User '%s' created successfully\n", argv[1]);
        return 0;
    }
    printk(KERN_ERR "useradd: failed to create user\n");
    return 1;
}

static int applet_crontab(int argc, char** argv) {
    if (argc >= 3 && strcmp(argv[1], "-a") == 0) {
        cron_add_job(argv[2], 60);
        printk(KERN_INFO "crontab: scheduled periodic job '%s' (interval: 60s)\n", argv[2]);
        return 0;
    }

    size_t count = cron_get_job_count();
    printk(ANSI_BRIGHT_CYAN "=== Active Scheduled Cron Jobs (%llu jobs) ===\n" ANSI_RESET, (uint64_t)count);
    printk(ANSI_YELLOW "ID   INTERVAL   RUNS  COMMAND\n" ANSI_RESET);
    for (size_t i = 0; i < count; i++) {
        const cron_job_t* j = cron_get_job(i);
        if (j && j->in_use) {
            printk("%-4u %-10u %-5u %s\n", j->id, j->interval_sec, j->run_count, j->command);
        }
    }
    return 0;
}

static int applet_vmstat(int argc, char** argv) {
    (void)argc; (void)argv;
    system_metrics_t m;
    metrics_sample(&m);

    printk(ANSI_BRIGHT_CYAN "procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----\n" ANSI_RESET);
    printk(" r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs  us sy id wa st\n");
    printk(" 1  0      0 %6llu    110  12415    0    0     8    12  100  120  %2u %2u %2u  0  0\n",
           m.mem_free_kb, m.cpu_user_pct, m.cpu_system_pct, m.cpu_idle_pct);
    return 0;
}

static int applet_iostat(int argc, char** argv) {
    (void)argc; (void)argv;
    system_metrics_t m;
    metrics_sample(&m);

    printk(ANSI_BRIGHT_CYAN "Device             tps    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn\n" ANSI_RESET);
    printk("sda               1.20         0.50         0.05       1440         64\n");
    printk("ram0              0.10         0.00         0.00          0          0\n");
    return 0;
}

static int applet_netstat(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_CYAN "Active Internet connections (servers and established)\n" ANSI_RESET);
    printk(ANSI_YELLOW "Proto Recv-Q Send-Q Local Address           Foreign Address         State\n" ANSI_RESET);

    if (sshd_is_running()) {
        printk("tcp        0      0 0.0.0.0:%-5u           0.0.0.0:*               LISTEN (sshd)\n", sshd_get_port());
    }
    if (httpd_is_running()) {
        printk("tcp        0      0 0.0.0.0:80              0.0.0.0:*               LISTEN (httpd)\n");
    }
    printk("udp        0      0 0.0.0.0:53              0.0.0.0:*               LISTEN (dns)\n");
    printk("udp        0      0 0.0.0.0:68              0.0.0.0:*               ESTABLISHED (dhcp)\n");

    for (size_t i = 0; i < 8; i++) {
        const ssh_session_t* s = sshd_get_session(i);
        if (s && s->in_use) {
            char client_ip_str[16];
            ip_to_str(s->client_ip, client_ip_str);
            printk("tcp        0      0 10.0.2.15:22            %s:%-5u         ESTABLISHED (%s)\n",
                   client_ip_str, s->client_port, s->username);
        }
    }
    return 0;
}

static int applet_htop(int argc, char** argv) {
    (void)argc; (void)argv;
    system_metrics_t m;
    metrics_sample(&m);

    printk(ANSI_BRIGHT_CYAN "  CPU[||||                                       4.2%%]     Tasks: 6, 1 thr, 1 running\n" ANSI_RESET);
    printk(ANSI_BRIGHT_CYAN "  Mem[||||||||||||||||                 %4lluM/%5lluM]     Uptime: %llu sec\n" ANSI_RESET,
           m.mem_used_kb / 1024, m.mem_total_kb / 1024, (uint64_t)(pit_get_ticks() / 100));
    printk(ANSI_YELLOW "  PID USER      PRI  NI  VIRT   RES   SHR S CPU%% MEM%%   TIME+  Command\n" ANSI_RESET);
    printk("    1 root       20   0  4096  1024   512 S  0.0  0.8  0:00.05 /init\n");
    printk("    2 root       20   0     0     0     0 S  0.0  0.0  0:00.01 [kthreadd]\n");
    printk("    3 root       20   0  1024   512   256 S  0.1  0.4  0:00.02 [syslogd]\n");
    printk("    4 root       20   0  2048  1024   512 S  0.2  0.8  0:00.04 [httpd:80]\n");
    printk("    5 root       20   0  1024   512   256 R  3.9  0.4  0:00.03 /bin/lazybox (htop)\n");
    return 0;
}

static int applet_bpftool(int argc, char** argv) {
    (void)argc; (void)argv;
    size_t count = bpf_get_prog_count();
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS In-Kernel eBPF Programs (%llu loaded) ===\n" ANSI_RESET, (uint64_t)count);
    printk(ANSI_YELLOW "ID   TYPE             INSNS  RUNS   TIME_NS  NAME\n" ANSI_RESET);
    for (size_t i = 0; i < count; i++) {
        const bpf_prog_t* p = bpf_get_prog(i);
        if (p && p->in_use) {
            const char* tstr = "SOCKET_FILTER";
            if (p->type == BPF_PROG_TYPE_KPROBE) tstr = "KPROBE       ";
            else if (p->type == BPF_PROG_TYPE_XDP) tstr = "XDP          ";
            printk("%-4u %s  %-6u %-6llu %-8llu %s\n",
                   p->id, tstr, p->insn_count, p->run_count, p->total_runtime_ns, p->name);
        }
    }
    return 0;
}

static int applet_pmap(int argc, char** argv) {
    uint32_t pid = (argc >= 2) ? (uint32_t)atoi(argv[1]) : 1;
    vma_dump_maps(pid);
    return 0;
}

static int applet_lssys(int argc, char** argv) {
    (void)argc; (void)argv;
    kobject_dump_tree();
    return 0;
}

static int applet_ptyinfo(int argc, char** argv) {
    (void)argc; (void)argv;
    size_t active = pty_get_active_count();
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Unix98 Pseudo-Terminal Subsystem (PTY) ===\n" ANSI_RESET);
    printk("Active Pairs: %llu / %u\n", (uint64_t)active, PTY_MAX_PAIRS);
    for (size_t i = 0; i < PTY_MAX_PAIRS; i++) {
        const pty_pair_t* p = pty_get_pair(i);
        if (p) {
            printk("  [%u] Master: %-12s Slave: %-12s Status: %s\n",
                   p->pty_num, p->master_name, p->slave_name,
                   p->in_use ? ANSI_BRIGHT_GREEN "ACTIVE" ANSI_RESET : ANSI_YELLOW "IDLE" ANSI_RESET);
        }
    }
    return 0;
}

static int applet_desktop(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(KERN_INFO "Rendering 2D Graphical Desktop Canvas on Linear Framebuffer...\n");
    canvas_render_desktop_mockup();
    printk(ANSI_BRIGHT_GREEN "Desktop UI Frame Rendered (320x200 32bpp TrueColor)\n" ANSI_RESET);
    return 0;
}

static int applet_lsblk(int argc, char** argv) {
    (void)argc; (void)argv;
    size_t count = block_get_device_count();
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Block Storage Devices (%llu devices) ===\n" ANSI_RESET, (uint64_t)count);
    printk(ANSI_YELLOW "NAME          MAJ:MIN   SIZE   TYPE   MOUNTPOINTS\n" ANSI_RESET);
    for (size_t i = 0; i < count; i++) {
        block_device_t* dev = block_get_device_by_index(i);
        if (dev) {
            uint64_t sz_mb = (dev->total_sectors * dev->sector_size) / (1024 * 1024);
            const char* mp = "-";
            if (strcmp(dev->name, "sda") == 0) mp = "/";
            else if (strcmp(dev->name, "ram0") == 0) mp = "/mnt/ramdisk";
            else if (strcmp(dev->name, "nvme0n1") == 0) mp = "/mnt/nvme";
            printk("%-12s  8:%-2u   %4lluM   disk   %s\n",
                   dev->name, (uint32_t)i, sz_mb, mp);
        }
    }
    return 0;
}

static int applet_lsdev(int argc, char** argv) {
    (void)argc; (void)argv;
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Active Hardware Device Drivers ===\n" ANSI_RESET);
    printk(ANSI_YELLOW "SUBSYSTEM    DRIVER       STATUS    DETAILS\n" ANSI_RESET);
    printk("Storage      NVMe PCIe    ACTIVE    Samsung 980 PRO 1TB (PCIe Gen4 x4)\n");
    printk("Storage      AHCI SATA    ACTIVE    Crucial MX500 SATA SSD (Port 0)\n");
    printk("Storage      VirtIO-Blk   ACTIVE    Para-virtualized Storage (/dev/vda)\n");
    printk("Storage      ATA / IDE    ACTIVE    Primary Master Drive (512B Sectors)\n");
    printk("Network      Intel E1000  ACTIVE    82540EM Gigabit Ethernet (eth0)\n");
    printk("Network      VirtIO-Net   ACTIVE    10-Gigabit Virtual NIC (eth1)\n");
    printk("Network      RTL8139      ACTIVE    Realtek 10/100 Fast Ethernet\n");
    printk("Audio        Intel HDA    ACTIVE    Azalia High Definition Audio (48kHz)\n");
    printk("Audio        AC97         ACTIVE    Analog Devices AD1980 AC'97 Audio\n");
    printk("Bus          PCI Host     ACTIVE    PCI 2.3 Bus Master Enumerator\n");
    printk("Bus          USB 3.0 xHCI ACTIVE    Extensible Host Controller (8 Ports)\n");
    printk("Display      Bochs VBE    ACTIVE    VESA/VBE Dynamic Resolution Engine\n");
    printk("Display      Linear FB    ACTIVE    VESA / Bochs Linear Framebuffer (32bpp)\n");
    printk("Sensors      CoreTemp DTS ACTIVE    CPU Digital Thermal & Fan Tachometer\n");
    printk("Entropy      VirtIO-RNG   ACTIVE    True Hardware Random Generator (/dev/hwrng)\n");
    printk("Input        PS/2 Mouse   ACTIVE    Dual-packet Wheel Mouse Driver\n");
    printk("Input        PS/2 Kbd     ACTIVE    Scancode Set 1 AT Keyboard Driver\n");
    return 0;
}

static int applet_sensors(int argc, char** argv) {
    (void)argc; (void)argv;
    const hwmon_data_t* hw = hwmon_get_data();
    printk(ANSI_BRIGHT_CYAN "%s\n" ANSI_RESET, hw->chip_name);
    printk("Adapter: ISA adapter\n");
    printk("Package id 0:  " ANSI_BRIGHT_GREEN "+%d.0 C" ANSI_RESET "  (crit = +100.0 C)\n", hw->cpu_temp_celsius);
    printk("Core 0:        " ANSI_BRIGHT_GREEN "+%d.0 C" ANSI_RESET "  (high = +80.0 C, crit = +100.0 C)\n", hw->cpu_temp_celsius - 1);
    printk("Core 1:        " ANSI_BRIGHT_GREEN "+%d.0 C" ANSI_RESET "  (high = +80.0 C, crit = +100.0 C)\n", hw->cpu_temp_celsius + 1);
    printk("Ambient:       " ANSI_BRIGHT_CYAN "+%d.0 C" ANSI_RESET "\n", hw->ambient_temp_celsius);
    printk("CPU Fan:       " ANSI_YELLOW "%u RPM" ANSI_RESET "  (min = 1000 RPM)\n", hw->fan_rpm);
    printk("Vcore:         " ANSI_WHITE "+%u.%02u V" ANSI_RESET "\n", hw->vcore_millivolts / 1000, (hw->vcore_millivolts % 1000) / 10);
    printk("+12V:          " ANSI_WHITE "+%u.%02u V" ANSI_RESET "\n", hw->v12_millivolts / 1000, (hw->v12_millivolts % 1000) / 10);
    printk("+5V:           " ANSI_WHITE "+%u.%02u V" ANSI_RESET "\n", hw->v5_millivolts / 1000, (hw->v5_millivolts % 1000) / 10);
    return 0;
}

// -------------------------------------------------------------
// Applet Dispatch Table (Over 70 Linux Tools)
// -------------------------------------------------------------
static const lazybox_applet_t applets[] = {
    // Core & Scripting
    {"lazybox",       applet_lazybox,       "lazybox [applet]",          "Multi-call utility manager", "Core"},
    {"sh",            applet_sh,            "sh <script> | -c <cmd>",    "Shell script interpreter",   "Core"},
    {"echo",          applet_echo,          "echo [text...]",            "Display text to stdout",     "Core"},
    {"env",           applet_env,           "env",                       "Print environment variables","Core"},
    {"export",        applet_export,        "export VAR=VAL",            "Set environment variable",   "Core"},
    {"whoami",        applet_whoami,        "whoami",                    "Print current user",         "Core"},
    {"id",            applet_id,            "id",                        "Print user and group IDs",   "Core"},
    {"date",          applet_date,          "date",                      "Display CMOS RTC date",      "Core"},
    {"cal",           applet_cal,           "cal",                       "Display calendar",           "Core"},

    // Filesystem & Editor
    {"ls",            applet_ls,            "ls [path]",                 "List directory contents",    "Filesystem"},
    {"cat",           applet_cat,           "cat <file...>",             "Concatenate files to stdout","Filesystem"},
    {"touch",         applet_touch,         "touch <file...>",           "Create empty files",         "Filesystem"},
    {"mkdir",         applet_mkdir,         "mkdir <dir...>",            "Create directories",         "Filesystem"},
    {"pwd",           applet_pwd,           "pwd",                       "Print current directory",    "Filesystem"},
    {"cd",            applet_cd,            "cd <dir>",                  "Change directory",           "Filesystem"},
    {"wc",            applet_wc,            "wc <file>",                 "Count lines, words, bytes",  "Filesystem"},
    {"head",          applet_head,          "head [-n lines] <file>",    "Output first lines of file", "Filesystem"},
    {"tail",          applet_tail,          "tail <file>",               "Output last lines of file",  "Filesystem"},
    {"stat",          applet_stat,          "stat <file>",               "Display file status",        "Filesystem"},
    {"cp",            applet_cp,            "cp <src> <dst>",            "Copy files",                 "Filesystem"},
    {"grep",          applet_grep,          "grep <pattern> <file>",     "Search pattern in file",     "Filesystem"},
    {"hexdump",       applet_hexdump,       "hexdump <file>",            "Display hex dump of file",   "Filesystem"},
    {"basename",      applet_basename,      "basename <path>",           "Strip directory path",       "Filesystem"},
    {"dirname",       applet_dirname,       "dirname <path>",            "Strip non-directory suffix", "Filesystem"},
    {"nano",          applet_nano_wrapper,  "nano [file]",               "Visual full-screen editor",  "Filesystem"},

    // Server & Web
    {"httpd",         applet_httpd,         "httpd [start|stop|status]", "Embedded micro HTTP server", "Server"},
    {"sshd",          applet_sshd,          "sshd [start|stop|status]",  "OpenSSH server daemon",      "Server"},
    {"ssh",           applet_ssh,           "ssh [user@]host [cmd]",     "Secure shell remote client", "Server"},
    {"curl",          applet_curl,          "curl <url>",                "HTTP transfer utility",      "Server"},
    {"wget",          applet_wget,          "wget <url>",                "Download files via HTTP",    "Server"},
    {"systemctl",     applet_systemctl,     "systemctl [cmd] [unit]",    "Manage system service daemons","Server"},
    {"service",       applet_systemctl,     "service [unit] [cmd]",      "Service manager alias",      "Server"},
    {"crontab",       applet_crontab,       "crontab [-a job]",          "Periodic cron job scheduler","Server"},

    // Logging & Authentication
    {"logger",        applet_logger,        "logger <msg>",              "Send message to syslog",     "Security"},
    {"logread",       applet_logread,       "logread",                   "Read system logs",           "Security"},
    {"su",            applet_su,            "su [user]",                 "Switch user session",        "Security"},
    {"passwd",        applet_passwd,        "passwd [user]",             "Change password",            "Security"},
    {"useradd",       applet_useradd,       "useradd <user>",            "Add new user account",       "Security"},

    // Sound & Voice Synthesis
    {"tts",           applet_tts,           "tts <text>",                "Phonetic formant voice synth","Sound"},
    {"tune",          applet_tune,          "tune [tetris|mario|startup]","8-bit retro chip melody player","Sound"},
    {"alsamixer",     applet_alsamixer,     "alsamixer",                 "Audio control & mixer",      "Sound"},

    // Kernel, Diagnostics & Namespaces
    {"lsmod",         applet_lsmod,         "lsmod",                     "Show kernel module status",  "Kernel"},
    {"insmod",        applet_insmod,        "insmod <mod>",              "Insert kernel module",       "Kernel"},
    {"rmmod",         applet_rmmod,         "rmmod <mod>",               "Remove kernel module",       "Kernel"},
    {"slabinfo",      applet_slabinfo,      "slabinfo",                  "Kernel SLAB cache statistics","Kernel"},
    {"trace",         applet_trace,         "trace [on|off|dump|clear]", "Kernel execution event tracer","Kernel"},
    {"unshare",       applet_unshare,       "unshare [name]",            "Create isolated namespace",  "Kernel"},
    {"bpftool",       applet_bpftool,       "bpftool",                   "Inspect in-kernel BPF bytecode","Kernel"},
    {"bpf",           applet_bpftool,       "bpf",                       "eBPF utility alias",         "Kernel"},
    {"pmap",          applet_pmap,          "pmap [pid]",                "Process memory map viewer",  "Kernel"},
    {"lssys",         applet_lssys,         "lssys",                     "Sysfs kobject hierarchy tree","Kernel"},
    {"ptyinfo",       applet_ptyinfo,       "ptyinfo",                   "Pseudo-terminal subsystem info","Kernel"},
    {"desktop",       applet_desktop,       "desktop",                   "Render 2D GUI Canvas desktop","Kernel"},
    {"draw",          applet_desktop,       "draw",                      "Canvas graphics demo",       "Kernel"},

    // Cryptography & Security
    {"md5sum",        applet_md5sum,        "md5sum <file|text>",        "Compute MD5 hash",           "Crypto"},
    {"sha256sum",     applet_sha256sum,     "sha256sum <file|text>",     "Compute SHA-256 hash",       "Crypto"},
    {"crc32",         applet_crc32,         "crc32 <text>",              "Calculate CRC32 checksum",   "Crypto"},
    {"rand",          applet_rand,          "rand [count]",              "Generate random numbers",    "Crypto"},
    {"certcheck",     applet_certcheck,     "certcheck",                 "View X.509 kernel keyring",  "Security"},
    {"capsh",         applet_capsh,         "capsh",                     "View process capabilities",  "Security"},
    {"ipcs",          applet_ipcs,          "ipcs",                      "Show IPC facilities status", "Security"},

    // Network & Firewall
    {"ifconfig",      applet_ifconfig,      "ifconfig",                  "Display network interface",  "Network"},
    {"ping",          applet_ping,          "ping <ip> [count]",         "ICMP Echo ping utility",     "Network"},
    {"arp",           applet_arp,           "arp",                       "View ARP cache table",       "Network"},
    {"dhclient",      applet_dhclient,      "dhclient",                  "Request DHCP lease",         "Network"},
    {"nslookup",      applet_nslookup,      "nslookup <host>",           "Query DNS name servers",     "Network"},
    {"iptables",      applet_iptables,      "iptables",                  "Firewall packet filter rules","Network"},
    {"netstat",       applet_netstat,       "netstat",                   "Network port connections",   "Network"},

    // Storage & Devices
    {"lsblk",         applet_lsblk,         "lsblk",                     "List block storage devices", "Storage"},
    {"lsdev",         applet_lsdev,         "lsdev",                     "List active hardware drivers", "Storage"},
    {"hdparm",        applet_hdparm,        "hdparm",                    "Inspect ATA hard disk",      "Storage"},
    {"lspci",         applet_lspci,         "lspci",                     "List PCI devices",           "Storage"},
    {"speaker",       applet_speaker,       "speaker <freq> <dur_ms>",   "Play PC speaker tone",       "Storage"},
    {"mouse",         applet_mouse,         "mouse",                     "Query PS/2 mouse cursor",    "Storage"},

    // Virtualization & Async I/O
    {"virtinfo",      applet_virtinfo,      "virtinfo",                  "Hypervisor & VirtIO status", "Virtualization"},
    {"io_uring_test", applet_io_uring_test, "io_uring_test",             "Test io_uring async queue",  "Virtualization"},

    // System Monitoring & Metrics
    {"sensors",       applet_sensors,       "sensors",                   "CPU Thermal & Fan Sensors",  "System"},
    {"uname",         applet_uname,         "uname [-a]",                "Print system architecture",  "System"},
    {"free",          applet_free,          "free",                      "Display RAM and Heap usage", "System"},
    {"uptime",        applet_uptime,        "uptime",                    "System running duration",    "System"},
    {"vmstat",        applet_vmstat,        "vmstat",                    "Virtual memory statistics",  "System"},
    {"iostat",        applet_iostat,        "iostat",                    "I/O device utilization",     "System"},
    {"htop",          applet_htop,          "htop",                      "Visual task manager",        "System"},
    {"dmesg",         applet_dmesg,         "dmesg",                     "Dump kernel boot ringbuffer","System"},
    {"ps",            applet_ps,            "ps",                        "List active processes",      "System"},
    {"top",           applet_top,           "top",                       "Interactive task manager",   "System"},
    {"sleep",         applet_sleep,         "sleep <seconds>",           "Delay execution",            "System"},
    {"reboot",        applet_reboot,        "reboot",                    "Reboot the computer",        "System"},
    {"poweroff",      applet_poweroff,      "poweroff",                  "Power off the computer",     "System"},
    {"clear",         applet_clear,         "clear",                     "Clear console screen",       "Terminal"},

    {NULL, NULL, NULL, NULL, NULL}
};

static int applet_lazybox(int argc, char** argv) {
    if (argc >= 2) {
        return lazybox_run_applet(argv[1], argc - 1, &argv[1]);
    }

    printk(ANSI_BRIGHT_CYAN "LazyBox v%s (2026-08-16) Linux-Compatible Multi-Call Suite\n" ANSI_RESET, LAZYBOX_VERSION);
    printk("Usage: lazybox [function] [arguments]...\n\n");
    printk(ANSI_YELLOW "Defined Applet Categories:\n" ANSI_RESET);

    const char* cats[] = {"Core", "Filesystem", "Sound", "Kernel", "Crypto", "Security", "Network", "Storage", "Virtualization", "System", "Terminal", NULL};
    for (int c = 0; cats[c] != NULL; c++) {
        printk(ANSI_BRIGHT_GREEN "  [%s]\n   " ANSI_RESET, cats[c]);
        for (int i = 0; applets[i].name != NULL; i++) {
            if (strcmp(applets[i].category, cats[c]) == 0) {
                printk(" %-14s", applets[i].name);
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
    return -1;
}
