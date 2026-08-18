#!/usr/bin/env python3
"""
SUB-OS Linux Kernel-Style Interactive Kconfig Configurator (lxdialog TUI/GUI)
Recreates the exact visual appearance, colors, floating dialogs, drop shadows,
and interactive controls of the Linux kernel 'make menuconfig' system.
"""

import sys
import os
import shutil
import subprocess

CONFIG_FILE = ".config"
AUTOCONF_HEADER = "include/config/autoconf.h"

DEFAULTS = {
    # Architecture
    "CONFIG_ARCH": "x86_64",
    "CONFIG_ARCH_X86_64": True,
    "CONFIG_ARCH_AARCH64": False,
    "CONFIG_ARCH_ARMV8I": False,

    # General Setup
    "CONFIG_SUBOS_HOSTNAME": "sub-os",
    "CONFIG_SUBOS_VERSION": "0.2.0-lts",
    "CONFIG_SMP": True,
    "CONFIG_PREEMPT": True,
    "CONFIG_OPTIMIZATION": "-O2",

    # Memory Management
    "CONFIG_MM_PMM": True,
    "CONFIG_MM_SLAB": True,
    "CONFIG_MM_VMA": True,

    # Filesystems
    "CONFIG_FS_VFS": True,
    "CONFIG_FS_FAT32": True,
    "CONFIG_FS_EXT2": True,
    "CONFIG_FS_RAMFS": True,
    "CONFIG_FS_DEVFS": True,
    "CONFIG_FS_PROCFS": True,
    "CONFIG_FS_SYSFS": True,

    # Storage Drivers
    "CONFIG_DRV_NVME": True,
    "CONFIG_DRV_AHCI": True,
    "CONFIG_DRV_ATA": True,
    "CONFIG_DRV_VIRTIO_BLK": True,
    "CONFIG_DRV_RAMDISK": True,

    # Network Drivers & Stack
    "CONFIG_NET_STACK": True,
    "CONFIG_DRV_E1000": True,
    "CONFIG_DRV_RTL8139": True,
    "CONFIG_DRV_VIRTIO_NET": True,
    "CONFIG_NET_HTTPD": True,
    "CONFIG_NET_SSHD": True,
    "CONFIG_NET_FILTER": True,

    # Audio & Display
    "CONFIG_DRV_SOUND_HDA": True,
    "CONFIG_DRV_SOUND_AC97": True,
    "CONFIG_DRV_BOCHS_VBE": True,
    "CONFIG_DRV_CANVAS_2D": True,

    # Bus, Sensors & Crypto
    "CONFIG_DRV_USB_XHCI": True,
    "CONFIG_DRV_HWMON": True,
    "CONFIG_DRV_VIRTIO_RNG": True,
    "CONFIG_DRV_PTY": True,

    # Security & Advanced
    "CONFIG_SECURITY_LSM": True,
    "CONFIG_SECURITY_KEYRING": True,
    "CONFIG_BPF_VM": True,
    "CONFIG_IO_URING": True,
    "CONFIG_USERLAND_LAZYBOX": True,
}

MENU_TREE = [
    {
        "id": "arch",
        "title": "Target architecture selection",
        "help": "Choose the target CPU instruction set architecture.\n\n"
                "  x86_64: 64-bit AMD64 / Intel 64 PC architecture.\n"
                "  aarch64: ARMv8-A 64-bit Cortex/Neoverse architecture.\n"
                "  armv8i: 32-bit ARM (AArch32) architecture.\n\n"
                "Symbol: CONFIG_ARCH",
        "items": [
            {"id": "CONFIG_ARCH_X86_64", "tag": "x86_64", "label": "x86_64 (64-Bit AMD64 / Intel 64)", "type": "radio", "group": "arch", "val": "x86_64",
             "help": "Builds for 64-bit x86 PCs with GDT, IDT, PIC, PIT, and Multiboot loader."},
            {"id": "CONFIG_ARCH_AARCH64", "tag": "aarch64", "label": "aarch64 (ARMv8-A 64-Bit Cortex-A57/Neoverse)", "type": "radio", "group": "arch", "val": "aarch64",
             "help": "Builds for 64-bit ARM with GICv2, Generic Timer, and 39-bit MMU."},
            {"id": "CONFIG_ARCH_ARMV8I", "tag": "armv8i", "label": "armv8i (32-Bit ARM / AArch32 Profile)", "type": "radio", "group": "arch", "val": "armv8i",
             "help": "Builds for 32-bit ARM with VMSAv7 Section MMU, GICv2, and VFP/NEON."},
        ]
    },
    {
        "id": "opt",
        "title": "Compiler optimizations and code generation",
        "help": "Select GCC optimization level for kernel compilation.\n\n"
                "  -O2: Recommended balanced speed and size optimization.\n"
                "  -O3: Aggressive auto-vectorization and loop unrolling.\n"
                "  -Os: Minimal binary footprint.\n"
                "  -O0: Debugging mode without register optimizations.\n\n"
                "Symbol: CONFIG_OPTIMIZATION",
        "items": [
            {"id": "CONFIG_OPT_O2", "tag": "O2", "label": "-O2 (High performance, recommended)", "type": "radio", "group": "opt", "val": "-O2", "help": "Standard kernel optimization level."},
            {"id": "CONFIG_OPT_O3", "tag": "O3", "label": "-O3 (Aggressive vectorization & loop unrolling)", "type": "radio", "group": "opt", "val": "-O3", "help": "Maximum speed optimization."},
            {"id": "CONFIG_OPT_OS", "tag": "Os", "label": "-Os (Optimize for minimal binary size)", "type": "radio", "group": "opt", "val": "-Os", "help": "Smallest kernel footprint."},
            {"id": "CONFIG_OPT_O0", "tag": "O0", "label": "-O0 (Disable optimizations for GDB debugging)", "type": "radio", "group": "opt", "val": "-O0", "help": "Unoptimized debug build."},
        ]
    },
    {
        "id": "general",
        "title": "General kernel setup and core subsystems",
        "help": "Configure core multitasking, synchronization, and kernel execution parameters.",
        "items": [
            {"id": "CONFIG_SMP", "tag": "SMP", "label": "Symmetric Multi-Processing (SMP) support", "type": "bool",
             "help": "Enables multi-core scheduler and per-CPU load balancing."},
            {"id": "CONFIG_PREEMPT", "tag": "PREEMPT", "label": "Preemptive kernel scheduling model", "type": "bool",
             "help": "Allows low-latency preemption for real-time task responsiveness."},
            {"id": "CONFIG_BPF_VM", "tag": "BPF_VM", "label": "In-Kernel eBPF register virtual machine", "type": "bool",
             "help": "Enables in-kernel safe bytecode execution for tracing and network filters."},
            {"id": "CONFIG_IO_URING", "tag": "IO_URING", "label": "High-performance io_uring ring engine", "type": "bool",
             "help": "Provides zero-syscall asynchronous submission and completion queues."},
        ]
    },
    {
        "id": "mm",
        "title": "Memory management & object allocators",
        "help": "Configure physical memory management, slab caches, and virtual address spaces.",
        "items": [
            {"id": "CONFIG_MM_PMM", "tag": "MM_PMM", "label": "Physical Page Frame Allocator (PMM)", "type": "bool",
             "help": "4KB bitmap-backed physical page frame allocator."},
            {"id": "CONFIG_MM_SLAB", "tag": "MM_SLAB", "label": "SLUB / SLAB high-speed object cache", "type": "bool",
             "help": "Fast object caches for kernel structures (VFS inodes, sockets, tasks)."},
            {"id": "CONFIG_MM_VMA", "tag": "MM_VMA", "label": "Virtual Memory Areas (VMA) and mmap subsystem", "type": "bool",
             "help": "Process address space mappings and demand paging."},
        ]
    },
    {
        "id": "fs",
        "title": "Virtual File Systems & Storage Device Drivers",
        "help": "Configure storage hardware controllers and mounted file system drivers.",
        "items": [
            {"id": "CONFIG_FS_VFS", "tag": "FS_VFS", "label": "Virtual File System (VFS) core framework", "type": "bool",
             "help": "Provides POSIX open, read, write, close, and mount abstraction."},
            {"id": "CONFIG_FS_FAT32", "tag": "FS_FAT32", "label": "FAT32 / VFAT file system driver", "type": "bool",
             "help": "High-performance FAT32 filesystem driver mounted at /mnt/fat32."},
            {"id": "CONFIG_FS_EXT2", "tag": "FS_EXT2", "label": "Second Extended (EXT2) Linux file system", "type": "bool",
             "help": "Standard Linux EXT2 filesystem driver with direct inode indexing."},
            {"id": "CONFIG_FS_SYSFS", "tag": "FS_SYSFS", "label": "Sysfs KObject virtual hierarchy (/sys)", "type": "bool",
             "help": "Exports kernel device tree and runtime telemetry to /sys."},
            {"id": "CONFIG_DRV_NVME", "tag": "DRV_NVME", "label": "NVM Express (NVMe PCIe SSD) driver", "type": "bool",
             "help": "High-throughput PCIe NVMe solid-state storage driver."},
            {"id": "CONFIG_DRV_AHCI", "tag": "DRV_AHCI", "label": "AHCI SATA 6Gb/s controller driver", "type": "bool",
             "help": "Serial ATA AHCI controller driver for hard drives and SSDs."},
            {"id": "CONFIG_DRV_ATA", "tag": "DRV_ATA", "label": "Legacy Parallel ATA / IDE storage driver", "type": "bool",
             "help": "IDE / PATA drive support for legacy hardware."},
            {"id": "CONFIG_DRV_VIRTIO_BLK", "tag": "DRV_VIRTIO_BLK", "label": "VirtIO paravirtualized block device (/dev/vda)", "type": "bool",
             "help": "Fast virtual storage device for QEMU / KVM hypervisors."},
            {"id": "CONFIG_DRV_RAMDISK", "tag": "DRV_RAMDISK", "label": "RAM disk block driver (/dev/ram0)", "type": "bool",
             "help": "In-memory dynamic block storage for initrd and testing."},
        ]
    },
    {
        "id": "net",
        "title": "Networking subsystem & hardware adapters",
        "help": "Configure network stack protocols, packet filtering, and ethernet adapters.",
        "items": [
            {"id": "CONFIG_NET_STACK", "tag": "NET_STACK", "label": "TCP/IP protocol stack (IPv4, UDP, TCP)", "type": "bool",
             "help": "Kernel socket networking stack with ARP, DHCP, DNS, UDP, and TCP."},
            {"id": "CONFIG_DRV_E1000", "tag": "DRV_E1000", "label": "Intel 82540EM Gigabit Ethernet NIC driver", "type": "bool",
             "help": "Standard Intel Gigabit PCI network interface card driver."},
            {"id": "CONFIG_DRV_RTL8139", "tag": "DRV_RTL8139", "label": "Realtek RTL8139 Fast Ethernet NIC driver", "type": "bool",
             "help": "100Mbps Realtek PCI network interface card driver."},
            {"id": "CONFIG_DRV_VIRTIO_NET", "tag": "DRV_VIRTIO_NET", "label": "VirtIO 10-Gigabit paravirtualized NIC", "type": "bool",
             "help": "Paravirtualized network adapter with zero-copy TX/RX rings."},
            {"id": "CONFIG_NET_FILTER", "tag": "NET_FILTER", "label": "NetFilter stateful packet firewall (iptables)", "type": "bool",
             "help": "Stateful packet inspection firewall and NAT engine."},
            {"id": "CONFIG_NET_HTTPD", "tag": "NET_HTTPD", "label": "In-kernel embedded HTTP REST web server", "type": "bool",
             "help": "Built-in HTTP server exposing REST APIs on port 80."},
            {"id": "CONFIG_NET_SSHD", "tag": "NET_SSHD", "label": "In-kernel Secure Shell (SSH 2.0) daemon", "type": "bool",
             "help": "Built-in encrypted SSH server on port 22 with auth."},
        ]
    },
    {
        "id": "drivers",
        "title": "Device Drivers, Graphics & Sound",
        "help": "Configure multimedia, sound cards, display adapters, and USB controllers.",
        "items": [
            {"id": "CONFIG_DRV_CANVAS_2D", "tag": "DRV_CANVAS_2D", "label": "2D TrueColor Rasterizer Canvas graphics engine", "type": "bool",
             "help": "Framebuffer rasterizer supporting lines, rectangles, and fonts."},
            {"id": "CONFIG_DRV_BOCHS_VBE", "tag": "DRV_BOCHS_VBE", "label": "Bochs / VBE dynamic resolution video adapter", "type": "bool",
             "help": "VBE/Bochs graphics controller for graphical resolution modes."},
            {"id": "CONFIG_DRV_SOUND_HDA", "tag": "DRV_SOUND_HDA", "label": "Intel High Definition Audio (Azalia HDA) driver", "type": "bool",
             "help": "PCI Intel HD Audio codec and stream controller."},
            {"id": "CONFIG_DRV_SOUND_AC97", "tag": "DRV_SOUND_AC97", "label": "Analog Devices AC'97 sound codec driver", "type": "bool",
             "help": "Legacy PCI AC'97 sound controller."},
            {"id": "CONFIG_DRV_USB_XHCI", "tag": "DRV_USB_XHCI", "label": "USB 3.0 Extensible Host Controller (xHCI)", "type": "bool",
             "help": "USB 3.0 Host Controller with device enumeration."},
            {"id": "CONFIG_DRV_HWMON", "tag": "DRV_HWMON", "label": "HWMON CPU temperature & fan sensor driver", "type": "bool",
             "help": "Hardware monitoring sensors for CPU temperature and fan RPM."},
            {"id": "CONFIG_DRV_VIRTIO_RNG", "tag": "DRV_VIRTIO_RNG", "label": "VirtIO true hardware random generator (/dev/hwrng)", "type": "bool",
             "help": "Paravirtualized entropy source feeding the kernel CSPRNG."},
            {"id": "CONFIG_DRV_PTY", "tag": "DRV_PTY", "label": "Unix98 Pseudo-Terminal (PTY) subsystem (/dev/pts)", "type": "bool",
             "help": "Pseudo-terminals for interactive terminal sessions and SSH."},
        ]
    },
    {
        "id": "security",
        "title": "Security options & Cryptographic Keyrings",
        "help": "Configure kernel security modules, authentication, and root keyrings.",
        "items": [
            {"id": "CONFIG_SECURITY_LSM", "tag": "SECURITY_LSM", "label": "Linux Security Module (LSM) access control", "type": "bool",
             "help": "Mandatory access control and POSIX capabilities enforcement."},
            {"id": "CONFIG_SECURITY_KEYRING", "tag": "SECURITY_KEYRING", "label": "X.509 cryptographic keyring & certificates", "type": "bool",
             "help": "System trust keyring verifying kernel modules and signatures."},
        ]
    },
    {
        "id": "userland",
        "title": "Userland Core & LazyBox Utility Suite",
        "help": "Configure userland binaries, shell applets, and command-line tools.",
        "items": [
            {"id": "CONFIG_USERLAND_LAZYBOX", "tag": "LAZYBOX", "label": "LazyBox multi-call utility suite (70+ Linux tools)", "type": "bool",
             "help": "Embedded BusyBox-like multi-call binary with sh, nano, neofetch, ls, etc."},
        ]
    }
]

def load_config():
    cfg = dict(DEFAULTS)
    if os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE, "r") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if "=" in line:
                    k, v = line.split("=", 1)
                    k = k.strip()
                    v = v.strip().strip('"')
                    if v == "y":
                        cfg[k] = True
                    elif v == "n":
                        cfg[k] = False
                    else:
                        cfg[k] = v
    return cfg

def save_config(cfg):
    os.makedirs(os.path.dirname(AUTOCONF_HEADER), exist_ok=True)
    with open(CONFIG_FILE, "w") as f_cfg, open(AUTOCONF_HEADER, "w") as f_hdr:
        f_cfg.write("# Automatically generated by SUB-OS Linux-Style Kconfig Configurator\n# Do not edit directly\n\n")
        f_hdr.write("/* Automatically generated by SUB-OS Kconfig. Do not edit. */\n#ifndef _AUTOCONF_H\n#define _AUTOCONF_H\n\n")

        # Write Architecture
        arch = cfg.get("CONFIG_ARCH", "x86_64")
        f_cfg.write(f"CONFIG_ARCH={arch}\n")
        f_hdr.write(f'#define CONFIG_ARCH "{arch}"\n')

        for k, v in sorted(cfg.items()):
            if k == "CONFIG_ARCH":
                continue
            if isinstance(v, bool):
                if v:
                    f_cfg.write(f"{k}=y\n")
                    f_hdr.write(f"#define {k} 1\n")
                else:
                    f_cfg.write(f"# {k} is not set\n")
            elif isinstance(v, str):
                f_cfg.write(f'{k}="{v}"\n')
                f_hdr.write(f'#define {k} "{v}"\n')
            elif isinstance(v, int):
                f_cfg.write(f"{k}={v}\n")
                f_hdr.write(f"#define {k} {v}\n")

        f_hdr.write("\n#endif /* _AUTOCONF_H */\n")

# -----------------------------------------------------------------------------
# Linux lxdialog Runner (Uses /usr/bin/dialog for authentic Linux GUI Look)
# -----------------------------------------------------------------------------
def run_dialog_gui():
    cfg = load_config()
    backtitle = "SUB-OS v0.2.0-lts Modular Monolithic Linux Kernel Configuration"

    current_menu_id = "main"

    while True:
        if current_menu_id == "main":
            # Build Main Menu Options
            menu_opts = []
            for menu in MENU_TREE:
                menu_opts.extend([menu["id"], f"{menu['title']}  --->"])

            cmd = [
                "dialog",
                "--clear",
                "--backtitle", backtitle,
                "--title", "SUB-OS Kernel Configuration",
                "--extra-button", "--extra-label", "Save",
                "--help-button",
                "--menu",
                "Arrow keys navigate the menu.  <Enter> selects submenus --->.\n"
                "Press <Save> to write .config, <Exit> to finish, <Help> for information.",
                "20", "78", "10"
            ] + menu_opts

            res = subprocess.run(cmd, stderr=subprocess.PIPE, text=True)
            ret = res.returncode
            sel = res.stderr.strip()

            if ret == 0:  # Select
                current_menu_id = sel
            elif ret == 1 or ret == 255:  # Exit or Esc
                # Confirmation dialog
                exit_cmd = [
                    "dialog", "--backtitle", backtitle,
                    "--title", "Save Kernel Configuration?",
                    "--yesno", "Do you wish to save your new kernel configuration?",
                    "7", "60"
                ]
                if subprocess.run(exit_cmd).returncode == 0:
                    save_config(cfg)
                break
            elif ret == 3:  # Extra button (Save)
                save_config(cfg)
                msg_cmd = ["dialog", "--backtitle", backtitle, "--msgbox", f"Configuration saved successfully to {CONFIG_FILE} and {AUTOCONF_HEADER}!", "7", "65"]
                subprocess.run(msg_cmd)
            elif ret == 2:  # Help button
                help_text = "SUB-OS Kernel Configuration Utility (lxdialog GUI)\n\nNavigate through each subsystem and configure features according to your target hardware needs."
                subprocess.run(["dialog", "--backtitle", backtitle, "--title", "Main Menu Help", "--msgbox", help_text, "12", "70"])

        else:
            # Submenu Display
            submenu = next((m for m in MENU_TREE if m["id"] == current_menu_id), None)
            if not submenu:
                current_menu_id = "main"
                continue

            items = submenu["items"]
            item_opts = []

            for item in items:
                itype = item.get("type")
                if itype == "bool":
                    status = "ON" if cfg.get(item["id"], False) else "OFF"
                    item_opts.extend([item["id"], item["label"], status])
                elif itype == "radio":
                    group = item.get("group")
                    if group == "arch":
                        status = "ON" if cfg.get("CONFIG_ARCH") == item["val"] else "OFF"
                    elif group == "opt":
                        status = "ON" if cfg.get("CONFIG_OPTIMIZATION") == item["val"] else "OFF"
                    else:
                        status = "OFF"
                    item_opts.extend([item["id"], item["label"], status])

            is_radiolist = any(i.get("type") == "radio" for i in items)
            dialog_type = "--radiolist" if is_radiolist else "--checklist"

            cmd = [
                "dialog",
                "--clear",
                "--backtitle", backtitle,
                "--title", submenu["title"],
                "--help-button",
                dialog_type,
                "Use <Space> to toggle features [*], <Enter> to confirm, <Help> for info.",
                "20", "78", "10"
            ] + item_opts

            res = subprocess.run(cmd, stderr=subprocess.PIPE, text=True)
            ret = res.returncode
            out = res.stderr.strip()

            if ret == 0:  # OK / Confirm
                selected_tags = [t.strip('"') for t in out.split()]
                if is_radiolist:
                    if selected_tags:
                        sel_id = selected_tags[0]
                        sel_item = next((i for i in items if i["id"] == sel_id), None)
                        if sel_item:
                            group = sel_item.get("group")
                            if group == "arch":
                                cfg["CONFIG_ARCH"] = sel_item["val"]
                                cfg["CONFIG_ARCH_X86_64"] = (sel_item["val"] == "x86_64")
                                cfg["CONFIG_ARCH_AARCH64"] = (sel_item["val"] == "aarch64")
                                cfg["CONFIG_ARCH_ARMV8I"] = (sel_item["val"] == "armv8i")
                            elif group == "opt":
                                cfg["CONFIG_OPTIMIZATION"] = sel_item["val"]
                else:
                    for item in items:
                        cfg[item["id"]] = (item["id"] in selected_tags)
                current_menu_id = "main"

            elif ret == 1 or ret == 255:  # Cancel or Esc
                current_menu_id = "main"

            elif ret == 2:  # Help button
                help_text = submenu.get("help", "No additional help available for this subsystem.")
                subprocess.run(["dialog", "--backtitle", backtitle, "--title", f"{submenu['title']} - Help", "--msgbox", help_text, "14", "72"])

    save_config(cfg)
    print(f"*** Configuration written to {CONFIG_FILE} and {AUTOCONF_HEADER} ***")

# -----------------------------------------------------------------------------
# Pure Python Curses GUI Renderer (Pixel-Perfect Linux lxdialog Replica)
# -----------------------------------------------------------------------------
def run_curses_gui(stdscr):
    import curses
    try:
        curses.curs_set(0)
    except Exception:
        pass
    try:
        curses.use_default_colors()
    except Exception:
        pass

    # Authentic Linux Kernel lxdialog Palette
    has_color = False
    try:
        if curses.has_colors():
            has_color = True
            curses.init_pair(1, curses.COLOR_WHITE, curses.COLOR_BLUE)    # Screen Backdrop
            curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_WHITE)   # Dialog Window Body
            curses.init_pair(3, curses.COLOR_YELLOW, curses.COLOR_WHITE)  # Window Title / Hotkeys
            curses.init_pair(4, curses.COLOR_WHITE, curses.COLOR_BLUE)    # Selected Highlighted Item
            curses.init_pair(5, curses.COLOR_BLUE, curses.COLOR_WHITE)    # Subtext / Checkbox Brackets
            curses.init_pair(6, curses.COLOR_BLACK, curses.COLOR_CYAN)    # Button Inverted Highlight
            curses.init_pair(7, curses.COLOR_BLACK, curses.COLOR_BLACK)   # Window Drop Shadow
    except Exception:
        has_color = False

    cfg = load_config()
    menu_stack = []
    current_menu = MENU_TREE
    cursor = 0
    btn_cursor = 0  # 0: Select, 1: Exit, 2: Help, 3: Save, 4: Defaults
    buttons = [" Select ", "  Exit  ", "  Help  ", "  Save  ", "Defaults"]

    while True:
        max_y, max_x = stdscr.getmaxyx()
        if max_y < 20 or max_x < 70:
            stdscr.clear()
            stdscr.addstr(0, 0, "Terminal window too small for menuconfig. Please resize!")
            stdscr.refresh()
            key = stdscr.getch()
            if key in [ord('q'), ord('Q'), 27]:
                break
            continue

        # 1. Fill Screen Backdrop (Royal Blue)
        stdscr.bkgd(' ', curses.color_pair(1) if has_color else 0)
        stdscr.clear()

        # Top Backtitle
        backtitle = "SUB-OS v0.2.0-lts Modular Monolithic Linux Kernel Configuration"
        stdscr.addstr(0, 2, backtitle, curses.A_BOLD)
        arch_info = f"Arch: [{cfg.get('CONFIG_ARCH', 'x86_64')}]  Opt: [{cfg.get('CONFIG_OPTIMIZATION', '-O2')}]"
        if max_x - len(arch_info) - 4 > len(backtitle) + 4:
            stdscr.addstr(0, max_x - len(arch_info) - 3, arch_info, curses.A_BOLD)

        # 2. Compute Floating Dialog Box Dimensions
        dw = min(78, max_x - 4)
        dh = min(22, max_y - 3)
        dx = (max_x - dw) // 2
        dy = (max_y - dh) // 2

        # 3. Draw 3D Drop Shadow
        if has_color:
            for sy in range(dy + 1, dy + dh + 1):
                stdscr.addstr(sy, dx + dw, "  ", curses.color_pair(7))
            stdscr.addstr(dy + dh, dx + 2, " " * (dw + 2), curses.color_pair(7))

        # 4. Draw Dialog Window Box
        box_attr = curses.color_pair(2) if has_color else 0
        for by in range(dy, dy + dh):
            stdscr.addstr(by, dx, " " * dw, box_attr)

        # Window Border
        title = " SUB-OS Kernel Configuration " if not menu_stack else f" {current_menu.get('title', 'Subsystem')} "
        for x in range(dx, dx + dw):
            stdscr.addch(dy, x, curses.ACS_HLINE, box_attr)
            stdscr.addch(dy + dh - 1, x, curses.ACS_HLINE, box_attr)
        for y in range(dy, dy + dh):
            stdscr.addch(y, dx, curses.ACS_VLINE, box_attr)
            stdscr.addch(y, dx + dw - 1, curses.ACS_VLINE, box_attr)
        stdscr.addch(dy, dx, curses.ACS_ULCORNER, box_attr)
        stdscr.addch(dy, dx + dw - 1, curses.ACS_URCORNER, box_attr)
        stdscr.addch(dy + dh - 1, dx, curses.ACS_LLCORNER, box_attr)
        stdscr.addch(dy + dh - 1, dx + dw - 1, curses.ACS_LRCORNER, box_attr)

        # Dialog Title Banner
        title_x = dx + (dw - len(title)) // 2
        stdscr.addstr(dy, title_x, title, (curses.color_pair(3) | curses.A_BOLD) if has_color else curses.A_BOLD)

        # Instructions Header
        inst1 = "Arrow keys navigate the menu.  <Enter> selects submenus --->."
        inst2 = "Press <Y> to include, <N> to exclude, <Space> to toggle [*]."
        stdscr.addstr(dy + 1, dx + 3, inst1[:dw - 6], box_attr)
        stdscr.addstr(dy + 2, dx + 3, inst2[:dw - 6], box_attr)

        # 5. Inner Scroll Box for Items
        items = current_menu if isinstance(current_menu, list) else current_menu.get("items", [])
        box_top = dy + 4
        box_height = dh - 8
        box_left = dx + 3
        box_width = dw - 6

        # Draw Inner Frame
        for by in range(box_top, box_top + box_height):
            stdscr.addstr(by, box_left, " " * box_width, box_attr)
        for x in range(box_left, box_left + box_width):
            stdscr.addch(box_top, x, curses.ACS_HLINE, box_attr)
            stdscr.addch(box_top + box_height - 1, x, curses.ACS_HLINE, box_attr)
        for y in range(box_top, box_top + box_height):
            stdscr.addch(y, box_left, curses.ACS_VLINE, box_attr)
            stdscr.addch(y, box_left + box_width - 1, curses.ACS_VLINE, box_attr)
        stdscr.addch(box_top, box_left, curses.ACS_ULCORNER, box_attr)
        stdscr.addch(box_top, box_left + box_width - 1, curses.ACS_URCORNER, box_attr)
        stdscr.addch(box_top + box_height - 1, box_left, curses.ACS_LLCORNER, box_attr)
        stdscr.addch(box_top + box_height - 1, box_left + box_width - 1, curses.ACS_LRCORNER, box_attr)

        visible_count = box_height - 2
        scroll_offset = max(0, cursor - visible_count + 1) if cursor >= visible_count else 0

        # Render Item Rows
        for i in range(visible_count):
            item_idx = scroll_offset + i
            if item_idx >= len(items):
                break
            item = items[item_idx]
            iy = box_top + 1 + i
            ix = box_left + 2

            is_cur = (item_idx == cursor)
            row_attr = (curses.color_pair(4) | curses.A_BOLD) if (is_cur and has_color) else (box_attr | curses.A_REVERSE if is_cur else box_attr)

            if "items" in item:  # Submenu
                display_text = f"  {item['title']}  --->"
            elif item.get("type") == "bool":
                val = cfg.get(item["id"], False)
                state = "[*]" if val else "[ ]"
                display_text = f"  {state} {item['label']}"
            elif item.get("type") == "radio":
                group = item.get("group")
                if group == "arch":
                    val = (cfg.get("CONFIG_ARCH") == item["val"])
                elif group == "opt":
                    val = (cfg.get("CONFIG_OPTIMIZATION") == item["val"])
                else:
                    val = False
                state = "(*)" if val else "( )"
                display_text = f"  {state} {item['label']}"
            else:
                display_text = f"  {item.get('title', '')}"

            row_str = display_text.ljust(box_width - 4)[:box_width - 4]
            stdscr.addstr(iy, ix, row_str, row_attr)

        # 6. Bottom Action Button Bar
        btn_y = dy + dh - 2
        btn_spacing = (dw - sum(len(b) + 4 for b in buttons)) // (len(buttons) + 1)
        cur_bx = dx + btn_spacing

        for b_idx, btn_name in enumerate(buttons):
            is_active_btn = (b_idx == btn_cursor)
            b_text = f"< {btn_name} >"
            b_attr = (curses.color_pair(6) | curses.A_BOLD) if (is_active_btn and has_color) else (curses.A_REVERSE if is_active_btn else box_attr)
            stdscr.addstr(btn_y, cur_bx, b_text, b_attr)
            cur_bx += len(b_text) + max(2, btn_spacing)

        stdscr.refresh()
        try:
            key = stdscr.getch()
        except Exception:
            break

        # Keyboard Navigation
        if key in [curses.KEY_UP, ord('k'), ord('K')]:
            if cursor > 0:
                cursor -= 1
        elif key in [curses.KEY_DOWN, ord('j'), ord('J')]:
            if cursor < len(items) - 1:
                cursor += 1
        elif key in [curses.KEY_LEFT, ord('h'), ord('H')]:
            btn_cursor = (btn_cursor - 1) % len(buttons)
        elif key in [curses.KEY_RIGHT, ord('l'), ord('L'), 9]:  # Tab or Right
            btn_cursor = (btn_cursor + 1) % len(buttons)
        elif key in [ord('y'), ord('Y')]:
            if cursor < len(items):
                sel = items[cursor]
                if sel.get("type") == "bool":
                    cfg[sel["id"]] = True
        elif key in [ord('n'), ord('N')]:
            if cursor < len(items):
                sel = items[cursor]
                if sel.get("type") == "bool":
                    cfg[sel["id"]] = False
        elif key == ord(' '):
            if cursor < len(items):
                sel = items[cursor]
                if sel.get("type") == "bool":
                    cfg[sel["id"]] = not cfg.get(sel["id"], False)
                elif sel.get("type") == "radio":
                    group = sel.get("group")
                    if group == "arch":
                        cfg["CONFIG_ARCH"] = sel["val"]
                        cfg["CONFIG_ARCH_X86_64"] = (sel["val"] == "x86_64")
                        cfg["CONFIG_ARCH_AARCH64"] = (sel["val"] == "aarch64")
                        cfg["CONFIG_ARCH_ARMV8I"] = (sel["val"] == "armv8i")
                    elif group == "opt":
                        cfg["CONFIG_OPTIMIZATION"] = sel["val"]
        elif key in [curses.KEY_ENTER, 10, 13]:
            # Trigger active bottom button
            if btn_cursor == 0:  # Select
                if cursor < len(items):
                    sel = items[cursor]
                    if "items" in sel:
                        menu_stack.append((current_menu, cursor))
                        current_menu = sel
                        cursor = 0
                    elif sel.get("type") == "bool":
                        cfg[sel["id"]] = not cfg.get(sel["id"], False)
                    elif sel.get("type") == "radio":
                        group = sel.get("group")
                        if group == "arch":
                            cfg["CONFIG_ARCH"] = sel["val"]
                            cfg["CONFIG_ARCH_X86_64"] = (sel["val"] == "x86_64")
                            cfg["CONFIG_ARCH_AARCH64"] = (sel["val"] == "aarch64")
                            cfg["CONFIG_ARCH_ARMV8I"] = (sel["val"] == "armv8i")
                        elif group == "opt":
                            cfg["CONFIG_OPTIMIZATION"] = sel["val"]
            elif btn_cursor == 1:  # Exit
                if menu_stack:
                    current_menu, cursor = menu_stack.pop()
                else:
                    save_config(cfg)
                    break
            elif btn_cursor == 2 or key == ord('?'):  # Help
                sel = items[cursor] if cursor < len(items) else None
                help_msg = sel.get("help", current_menu.get("help", "No help available.")) if sel else "SUB-OS Kernel Configurator"
                show_popup(stdscr, "Option Help", help_msg, max_y, max_x)
            elif btn_cursor == 3:  # Save
                save_config(cfg)
                show_popup(stdscr, "Save Configuration", f"Configuration successfully saved to {CONFIG_FILE} and {AUTOCONF_HEADER}!", max_y, max_x)
            elif btn_cursor == 4:  # Defaults
                cfg = dict(DEFAULTS)
                save_config(cfg)
                show_popup(stdscr, "Reset Defaults", "Default configuration options loaded!", max_y, max_x)
        elif key in [ord('s'), ord('S')]:
            save_config(cfg)
            show_popup(stdscr, "Save Configuration", f"Configuration successfully saved to {CONFIG_FILE} and {AUTOCONF_HEADER}!", max_y, max_x)
        elif key in [ord('q'), ord('Q'), 27]:  # Esc or Q
            if menu_stack:
                current_menu, cursor = menu_stack.pop()
            else:
                save_config(cfg)
                break

def show_popup(stdscr, title, message, max_y, max_x):
    pw = min(64, max_x - 6)
    ph = min(12, max_y - 6)
    px = (max_x - pw) // 2
    py = (max_y - ph) // 2
    import curses

    # Draw popup box
    for y in range(py, py + ph):
        stdscr.addstr(y, px, " " * pw, curses.A_REVERSE)
    for x in range(px, px + pw):
        stdscr.addch(py, x, curses.ACS_HLINE, curses.A_REVERSE)
        stdscr.addch(py + ph - 1, x, curses.ACS_HLINE, curses.A_REVERSE)
    for y in range(py, py + ph):
        stdscr.addch(y, px, curses.ACS_VLINE, curses.A_REVERSE)
        stdscr.addch(y, px + pw - 1, curses.ACS_VLINE, curses.A_REVERSE)

    stdscr.addstr(py, px + (pw - len(title) - 2) // 2, f" {title} ", curses.A_BOLD | curses.A_REVERSE)

    # Word wrap message lines
    lines = []
    for raw_line in message.split("\n"):
        while len(raw_line) > pw - 4:
            split_idx = raw_line[:pw - 4].rfind(" ")
            if split_idx <= 0:
                split_idx = pw - 4
            lines.append(raw_line[:split_idx])
            raw_line = raw_line[split_idx:].lstrip()
        lines.append(raw_line)

    for i, line in enumerate(lines[:ph - 4]):
        stdscr.addstr(py + 2 + i, px + 2, line[:pw - 4], curses.A_REVERSE)

    btn_str = "< OK >"
    stdscr.addstr(py + ph - 2, px + (pw - len(btn_str)) // 2, btn_str, curses.A_BOLD)
    stdscr.refresh()
    stdscr.getch()

def defconfig(arch="x86_64"):
    cfg = dict(DEFAULTS)
    cfg["CONFIG_ARCH"] = arch
    if arch == "x86_64":
        cfg["CONFIG_ARCH_X86_64"] = True
        cfg["CONFIG_ARCH_AARCH64"] = False
        cfg["CONFIG_ARCH_ARMV8I"] = False
    elif arch in ["aarch64", "arm64"]:
        cfg["CONFIG_ARCH_X86_64"] = False
        cfg["CONFIG_ARCH_AARCH64"] = True
        cfg["CONFIG_ARCH_ARMV8I"] = False
    elif arch in ["armv8i", "arm32", "arm", "armv8"]:
        cfg["CONFIG_ARCH_X86_64"] = False
        cfg["CONFIG_ARCH_AARCH64"] = False
        cfg["CONFIG_ARCH_ARMV8I"] = True
    save_config(cfg)
    print(f"*** Default configuration for '{arch}' written to {CONFIG_FILE} and {AUTOCONF_HEADER} ***")

def main():
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
        if cmd in ["--defconfig", "defconfig"]:
            arch = sys.argv[2] if len(sys.argv) > 2 else "x86_64"
            defconfig(arch)
            return
        elif cmd == "--x86_64":
            defconfig("x86_64")
            return
        elif cmd in ["--aarch64", "--armv8"]:
            defconfig("aarch64")
            return
        elif cmd == "--armv8i":
            defconfig("armv8i")
            return

    # Check for /usr/bin/dialog for authentic Linux lxdialog GUI
    if shutil.which("dialog") and sys.stdin.isatty():
        try:
            run_dialog_gui()
            return
        except Exception:
            pass

    # Fallback to Curses GUI replica
    try:
        import curses
        curses.wrapper(run_curses_gui)
        print(f"*** Configuration saved to {CONFIG_FILE} and {AUTOCONF_HEADER} ***")
    except Exception:
        # Fallback if no TTY
        cfg = load_config()
        save_config(cfg)
        print(f"*** Configuration written to {CONFIG_FILE} and {AUTOCONF_HEADER} ***")

if __name__ == "__main__":
    main()
