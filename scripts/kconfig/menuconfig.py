#!/usr/bin/env python3
"""
SUB-OS Linux-Style Interactive Kconfig Menuconfig (TUI)
Provides a full curses-based terminal configuration UI matching Linux menuconfig.
Includes robust error-handling for all terminal types and ANSI fallback.
Generates .config and include/config/autoconf.h.
"""

import sys
import os

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

MENU_STRUCTURE = [
    {
        "title": "Target Architecture Selection",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_ARCH_X86_64", "label": "x86_64 (64-Bit AMD64/Intel x86-64)", "type": "radio", "group": "arch", "val": "x86_64"},
            {"name": "CONFIG_ARCH_AARCH64", "label": "aarch64 (ARMv8-A 64-Bit Cortex/Neoverse)", "type": "radio", "group": "arch", "val": "aarch64"},
            {"name": "CONFIG_ARCH_ARMV8I", "label": "armv8i (32-Bit ARM/AArch32 Profile)", "type": "radio", "group": "arch", "val": "armv8i"},
        ]
    },
    {
        "title": "Compiler Optimizations & Code Generation",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_OPT_O2", "label": "-O2 (Recommended: High Performance Optimization)", "type": "radio", "group": "opt", "val": "-O2"},
            {"name": "CONFIG_OPT_O3", "label": "-O3 (Aggressive Vectorization & Loop Unrolling)", "type": "radio", "group": "opt", "val": "-O3"},
            {"name": "CONFIG_OPT_OS", "label": "-Os (Optimize for Smallest Binary Footprint)", "type": "radio", "group": "opt", "val": "-Os"},
            {"name": "CONFIG_OPT_O0", "label": "-O0 (Disable Optimizations for GDB Debugging)", "type": "radio", "group": "opt", "val": "-O0"},
        ]
    },
    {
        "title": "General Kernel Setup",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_SMP", "label": "Symmetric Multi-Processing (SMP) Support", "type": "bool"},
            {"name": "CONFIG_PREEMPT", "label": "Preemptive Kernel Scheduling Model", "type": "bool"},
            {"name": "CONFIG_BPF_VM", "label": "In-Kernel eBPF Register Virtual Machine", "type": "bool"},
            {"name": "CONFIG_IO_URING", "label": "High-Performance io_uring Ring Buffers", "type": "bool"},
        ]
    },
    {
        "title": "Memory Management & Object Allocators",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_MM_PMM", "label": "Physical Page Frame Allocator (PMM)", "type": "bool"},
            {"name": "CONFIG_MM_SLAB", "label": "High-Performance SLUB/SLAB Object Cache", "type": "bool"},
            {"name": "CONFIG_MM_VMA", "label": "Virtual Memory Areas (VMA) & mmap Subsystem", "type": "bool"},
        ]
    },
    {
        "title": "Virtual File Systems & Storage Drivers",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_FS_VFS", "label": "Virtual File System (VFS) Core Subsystem", "type": "bool"},
            {"name": "CONFIG_FS_FAT32", "label": "FAT32 / VFAT File System Driver", "type": "bool"},
            {"name": "CONFIG_FS_EXT2", "label": "Second Extended (EXT2) Linux File System", "type": "bool"},
            {"name": "CONFIG_FS_SYSFS", "label": "Sysfs KObject Virtual Hierarchy (/sys)", "type": "bool"},
            {"name": "CONFIG_DRV_NVME", "label": "NVM Express (NVMe PCIe SSD) Driver", "type": "bool"},
            {"name": "CONFIG_DRV_AHCI", "label": "AHCI SATA 6Gb/s SSD/HDD Controller Driver", "type": "bool"},
            {"name": "CONFIG_DRV_ATA", "label": "Legacy IDE/ATA Parallel Storage Driver", "type": "bool"},
            {"name": "CONFIG_DRV_VIRTIO_BLK", "label": "VirtIO Paravirtualized Block Storage (/dev/vda)", "type": "bool"},
            {"name": "CONFIG_DRV_RAMDISK", "label": "In-Memory Virtual Ramdisk Storage (/dev/ram0)", "type": "bool"},
        ]
    },
    {
        "title": "Network Subsystem & Hardware Adapters",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_NET_STACK", "label": "TCP/IP Network Stack (IPv4, UDP, TCP)", "type": "bool"},
            {"name": "CONFIG_DRV_E1000", "label": "Intel 82540EM Gigabit Ethernet NIC Driver", "type": "bool"},
            {"name": "CONFIG_DRV_RTL8139", "label": "Realtek RTL8139 Fast Ethernet NIC Driver", "type": "bool"},
            {"name": "CONFIG_DRV_VIRTIO_NET", "label": "VirtIO 10-Gigabit Paravirtualized NIC", "type": "bool"},
            {"name": "CONFIG_NET_FILTER", "label": "NetFilter Stateful Packet Firewall (iptables)", "type": "bool"},
            {"name": "CONFIG_NET_HTTPD", "label": "In-Kernel Embedded HTTP REST Web Server", "type": "bool"},
            {"name": "CONFIG_NET_SSHD", "label": "In-Kernel Secure Shell (SSH 2.0) Daemon", "type": "bool"},
        ]
    },
    {
        "title": "Device Drivers & Multimedia",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_DRV_CANVAS_2D", "label": "2D TrueColor Rasterizer Canvas Graphics Engine", "type": "bool"},
            {"name": "CONFIG_DRV_BOCHS_VBE", "label": "Bochs / VBE Dynamic Resolution Display Adapter", "type": "bool"},
            {"name": "CONFIG_DRV_SOUND_HDA", "label": "Intel High Definition Audio (Azalia HDA) Driver", "type": "bool"},
            {"name": "CONFIG_DRV_SOUND_AC97", "label": "Analog Devices AC'97 Sound Codec Driver", "type": "bool"},
            {"name": "CONFIG_DRV_USB_XHCI", "label": "USB 3.0 Extensible Host Controller (xHCI)", "type": "bool"},
            {"name": "CONFIG_DRV_HWMON", "label": "HWMON CPU CoreTemp & Fan Tachometer Sensors", "type": "bool"},
            {"name": "CONFIG_DRV_VIRTIO_RNG", "label": "VirtIO True Hardware Random Generator (/dev/hwrng)", "type": "bool"},
            {"name": "CONFIG_DRV_PTY", "label": "Unix98 Pseudo-Terminal (PTY) Subsystem (/dev/pts)", "type": "bool"},
        ]
    },
    {
        "title": "Security, LSM & Cryptographic Keyrings",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_SECURITY_LSM", "label": "Linux Security Module (LSM) Mandatory Access Control", "type": "bool"},
            {"name": "CONFIG_SECURITY_KEYRING", "label": "X.509 Cryptographic Keyring & Certificates", "type": "bool"},
        ]
    },
    {
        "title": "Userland Core & LazyBox Utility Suite",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_USERLAND_LAZYBOX", "label": "LazyBox Multi-Call Userland Utility Suite (70+ Tools)", "type": "bool"},
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
        f_cfg.write("# Automatically generated by SUB-OS Kconfig TUI Configurator\n# Do not edit directly\n\n")
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

def run_curses_tui(stdscr):
    import curses
    try:
        curses.curs_set(0)
    except Exception:
        pass

    try:
        curses.use_default_colors()
    except Exception:
        pass

    has_color = False
    try:
        if curses.has_colors():
            has_color = True
            curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_CYAN)    # Header / Bar
            curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLUE)    # Main background
            curses.init_pair(3, curses.COLOR_YELLOW, curses.COLOR_BLUE)   # Highlight title
            curses.init_pair(4, curses.COLOR_BLACK, curses.COLOR_WHITE)   # Selected row
            curses.init_pair(5, curses.COLOR_CYAN, curses.COLOR_BLUE)     # Subtext
    except Exception:
        has_color = False

    cfg = load_config()
    current_menu = MENU_STRUCTURE
    menu_stack = []
    cursor_stack = [0]

    while True:
        if has_color:
            try:
                stdscr.bkgd(' ', curses.color_pair(2))
            except Exception:
                pass
        stdscr.clear()
        max_y, max_x = stdscr.getmaxyx()

        # Top Header Bar
        title_str = " SUB-OS Kernel v0.2.0 Configuration (Linux-Style Kconfig TUI) "
        if has_color:
            stdscr.attron(curses.color_pair(1) | curses.A_BOLD)
            stdscr.addstr(0, 0, title_str.ljust(max_x)[:max_x])
            stdscr.attroff(curses.color_pair(1) | curses.A_BOLD)
        else:
            stdscr.addstr(0, 0, title_str.ljust(max_x)[:max_x])

        # Active Architecture Indicator
        cur_arch = cfg.get("CONFIG_ARCH", "x86_64")
        cur_opt = cfg.get("CONFIG_OPTIMIZATION", "-O2")
        info_str = f" Target Architecture: [{cur_arch}]  |  Optimization: [{cur_opt}] "
        if has_color:
            stdscr.attron(curses.color_pair(3))
            stdscr.addstr(1, 2, info_str[:max_x-4])
            stdscr.attroff(curses.color_pair(3))
        else:
            stdscr.addstr(1, 2, info_str[:max_x-4])

        # Instructions / Help
        nav_help = "Arrow Keys / jk navigate | <Enter> / <Space> Toggle | [S]ave | [Q] Back/Exit"
        if has_color:
            stdscr.attron(curses.color_pair(5))
            stdscr.addstr(2, 2, nav_help[:max_x-4])
            stdscr.attroff(curses.color_pair(5))
        else:
            stdscr.addstr(2, 2, nav_help[:max_x-4])

        try:
            stdscr.hline(3, 1, curses.ACS_HLINE, max_x - 2)
        except Exception:
            pass

        # Menu Items
        items = current_menu if isinstance(current_menu, list) else current_menu.get("items", [])
        cursor = cursor_stack[-1]
        start_y = 5

        for idx, item in enumerate(items):
            y = start_y + idx
            if y >= max_y - 3:
                break

            prefix = "    "
            is_selected = (idx == cursor)

            if item.get("type") == "submenu":
                line_text = f"---> {item['title']}"
            elif item.get("type") == "bool":
                val = cfg.get(item["name"], False)
                state = "[*]" if val else "[ ]"
                line_text = f"{state} {item['label']}"
            elif item.get("type") == "radio":
                group = item.get("group")
                if group == "arch":
                    val = (cfg.get("CONFIG_ARCH") == item["val"])
                elif group == "opt":
                    val = (cfg.get("CONFIG_OPTIMIZATION") == item["val"])
                else:
                    val = False
                state = "(*)" if val else "( )"
                line_text = f"{state} {item['label']}"
            else:
                line_text = item.get("title", "")

            display_line = f"{prefix}{line_text}".ljust(max_x - 6)[:max_x - 6]

            if is_selected:
                if has_color:
                    stdscr.attron(curses.color_pair(4) | curses.A_BOLD)
                    stdscr.addstr(y, 2, f" {display_line} ")
                    stdscr.attroff(curses.color_pair(4) | curses.A_BOLD)
                else:
                    stdscr.addstr(y, 2, f"> {display_line} ")
            else:
                if has_color:
                    stdscr.attron(curses.color_pair(2))
                    stdscr.addstr(y, 2, f" {display_line} ")
                    stdscr.attroff(curses.color_pair(2))
                else:
                    stdscr.addstr(y, 2, f"  {display_line} ")

        # Bottom Status Bar
        bottom_bar = " <Save> [S]   <Exit> [Q]   <Defaults> [D] "
        if has_color:
            stdscr.attron(curses.color_pair(1))
            stdscr.addstr(max_y - 1, 0, bottom_bar.ljust(max_x)[:max_x])
            stdscr.attroff(curses.color_pair(1))
        else:
            stdscr.addstr(max_y - 1, 0, bottom_bar.ljust(max_x)[:max_x])

        stdscr.refresh()
        try:
            key = stdscr.getch()
        except Exception:
            break

        if key in [curses.KEY_UP, ord('k'), ord('K')]:
            if cursor_stack[-1] > 0:
                cursor_stack[-1] -= 1
        elif key in [curses.KEY_DOWN, ord('j'), ord('J')]:
            if cursor_stack[-1] < len(items) - 1:
                cursor_stack[-1] += 1
        elif key in [curses.KEY_ENTER, 10, 13, ord(' ')]:
            sel_item = items[cursor_stack[-1]]
            if sel_item.get("type") == "submenu":
                menu_stack.append(current_menu)
                cursor_stack.append(0)
                current_menu = sel_item
            elif sel_item.get("type") == "bool":
                name = sel_item["name"]
                cfg[name] = not cfg.get(name, False)
            elif sel_item.get("type") == "radio":
                group = sel_item.get("group")
                if group == "arch":
                    cfg["CONFIG_ARCH"] = sel_item["val"]
                    cfg["CONFIG_ARCH_X86_64"] = (sel_item["val"] == "x86_64")
                    cfg["CONFIG_ARCH_AARCH64"] = (sel_item["val"] == "aarch64")
                    cfg["CONFIG_ARCH_ARMV8I"] = (sel_item["val"] == "armv8i")
                elif group == "opt":
                    cfg["CONFIG_OPTIMIZATION"] = sel_item["val"]
        elif key in [ord('s'), ord('S')]:
            save_config(cfg)
            if has_color:
                stdscr.attron(curses.color_pair(1) | curses.A_BOLD)
                stdscr.addstr(max_y - 2, 4, f" Saved configuration! Press any key... ")
                stdscr.attroff(curses.color_pair(1) | curses.A_BOLD)
            else:
                stdscr.addstr(max_y - 2, 4, f" Saved configuration! Press any key... ")
            stdscr.refresh()
            stdscr.getch()
        elif key in [ord('d'), ord('D')]:
            cfg = dict(DEFAULTS)
            save_config(cfg)
            if has_color:
                stdscr.attron(curses.color_pair(1) | curses.A_BOLD)
                stdscr.addstr(max_y - 2, 4, " Loaded default configuration! Press any key... ")
                stdscr.attroff(curses.color_pair(1) | curses.A_BOLD)
            else:
                stdscr.addstr(max_y - 2, 4, " Loaded default configuration! Press any key... ")
            stdscr.refresh()
            stdscr.getch()
        elif key in [ord('q'), ord('Q'), 27]: # Esc or Q
            if menu_stack:
                current_menu = menu_stack.pop()
                cursor_stack.pop()
            else:
                save_config(cfg)
                break

def run_ansi_fallback():
    """Text-based interactive configurator for non-curses environments."""
    cfg = load_config()
    print("\n=================================================================")
    print(" SUB-OS Interactive Kernel Configuration (ANSI Mode)")
    print("=================================================================")
    print(f" Current Architecture: {cfg.get('CONFIG_ARCH', 'x86_64')}")
    print(f" Optimization Level:   {cfg.get('CONFIG_OPTIMIZATION', '-O2')}")
    print("\nSelect an option to configure:")
    print(" 1) Architecture: x86_64 (64-Bit AMD64/Intel)")
    print(" 2) Architecture: aarch64 (ARMv8-A 64-Bit)")
    print(" 3) Architecture: armv8i (32-Bit ARM/AArch32)")
    print(" 4) Toggle Core Subsystems")
    print(" 5) Save and Exit")
    print(" 6) Exit without saving\n")

    try:
        choice = input("Enter choice [1-6] (Default: 5): ").strip()
    except EOFError:
        choice = "5"

    if choice == "1":
        defconfig("x86_64")
    elif choice == "2":
        defconfig("aarch64")
    elif choice == "3":
        defconfig("armv8i")
    elif choice == "5" or not choice:
        save_config(cfg)
        print(f"*** Configuration saved to {CONFIG_FILE} and {AUTOCONF_HEADER} ***")
    else:
        save_config(cfg)

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

    # Try curses TUI first, gracefully fall back to ANSI mode if curses is unavailable
    try:
        import curses
        curses.wrapper(run_curses_tui)
        print(f"*** Configuration saved to {CONFIG_FILE} and {AUTOCONF_HEADER} ***")
    except Exception as e:
        run_ansi_fallback()

if __name__ == "__main__":
    main()
