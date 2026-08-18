#!/usr/bin/env python3
"""
SUB-OS Linux-Style Interactive Kconfig Menuconfig (TUI)
Provides a full curses-based terminal configuration UI matching Linux menuconfig.
Generates .config and include/config/autoconf.h.
"""

import sys
import os
import curses

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
            {"name": "CONFIG_ARCH_ARMV8I", "label": "armv8i (ARMv8 Instruction Set Profile)", "type": "radio", "group": "arch", "val": "armv8i"},
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
        "title": "Virtual File Systems & Storage Drivers",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_FS_FAT32", "label": "FAT32 / VFAT File System Driver", "type": "bool"},
            {"name": "CONFIG_FS_EXT2", "label": "Second Extended (EXT2) Linux File System", "type": "bool"},
            {"name": "CONFIG_FS_SYSFS", "label": "Sysfs KObject Virtual Hierarchy (/sys)", "type": "bool"},
            {"name": "CONFIG_DRV_NVME", "label": "NVM Express (NVMe PCIe SSD) Driver", "type": "bool"},
            {"name": "CONFIG_DRV_AHCI", "label": "AHCI SATA 6Gb/s SSD/HDD Controller Driver", "type": "bool"},
            {"name": "CONFIG_DRV_VIRTIO_BLK", "label": "VirtIO Paravirtualized Block Storage (/dev/vda)", "type": "bool"},
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
            {"name": "CONFIG_NET_SSHD", "label": "In-Kernel Secure Shell (SSH) Daemon", "type": "bool"},
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
        "title": "Security, LSM & Keyrings",
        "type": "submenu",
        "items": [
            {"name": "CONFIG_SECURITY_LSM", "label": "Linux Security Module (LSM) Mandatory Access Control", "type": "bool"},
            {"name": "CONFIG_SECURITY_KEYRING", "label": "X.509 Cryptographic Keyring & Certificates", "type": "bool"},
        ]
    },
    {
        "title": "Userland Core & LazyBox",
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

def run_tui(stdscr):
    curses.curs_set(0)
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_CYAN)    # Header / Bar
    curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLUE)    # Main background
    curses.init_pair(3, curses.COLOR_YELLOW, curses.COLOR_BLUE)   # Highlight title
    curses.init_pair(4, curses.COLOR_BLACK, curses.COLOR_WHITE)   # Selected row
    curses.init_pair(5, curses.COLOR_CYAN, curses.COLOR_BLUE)     # Subtext

    cfg = load_config()
    current_menu = MENU_STRUCTURE
    menu_stack = []
    cursor_stack = [0]

    while True:
        stdscr.bkgd(' ', curses.color_pair(2))
        stdscr.clear()
        max_y, max_x = stdscr.getmaxyx()

        # Top Banner
        header = f" SUB-OS v{cfg.get('CONFIG_SUBOS_VERSION', '0.2.0-lts')} ({cfg.get('CONFIG_ARCH', 'x86_64')}) Linux-Style Kernel Configuration "
        stdscr.attron(curses.color_pair(1) | curses.A_BOLD)
        stdscr.addstr(0, 0, header.center(max_x - 1)[:max_x - 1])
        stdscr.attroff(curses.color_pair(1) | curses.A_BOLD)

        # Subtitle
        stdscr.attron(curses.color_pair(3) | curses.A_BOLD)
        title_text = "Main Menu" if not menu_stack else menu_stack[-1]["title"]
        stdscr.addstr(2, 4, f"-> {title_text}")
        stdscr.attroff(curses.color_pair(3) | curses.A_BOLD)

        # Instructions
        stdscr.attron(curses.color_pair(5))
        stdscr.addstr(3, 4, "Use [UP/DOWN] arrows, [SPACE] to toggle [*], [ENTER] for submenu, [S] to Save, [Q] to Exit")
        stdscr.attroff(curses.color_pair(5))

        # Items list
        items = current_menu if not menu_stack else menu_stack[-1]["items"]
        cur_idx = cursor_stack[-1]
        if cur_idx >= len(items):
            cur_idx = len(items) - 1
        if cur_idx < 0:
            cur_idx = 0
        cursor_stack[-1] = cur_idx

        box_top = 5
        box_height = max_y - 8

        for i, item in enumerate(items):
            if i >= box_height:
                break
            y = box_top + i
            is_sel = (i == cur_idx)

            if "items" in item: # Submenu
                display = f"   ---> {item['title']}"
            elif item.get("type") == "bool":
                val = cfg.get(item["name"], False)
                mark = "[*]" if val else "[ ]"
                display = f"   {mark} {item['label']}"
            elif item.get("type") == "radio":
                group_val = cfg.get("CONFIG_ARCH", "x86_64")
                mark = "(*)" if group_val == item["val"] else "( )"
                display = f"   {mark} {item['label']}"
            else:
                display = f"   {item.get('label', item.get('title', ''))}"

            if is_sel:
                stdscr.attron(curses.color_pair(4) | curses.A_BOLD)
                stdscr.addstr(y, 4, display.ljust(max_x - 10)[:max_x - 10])
                stdscr.attroff(curses.color_pair(4) | curses.A_BOLD)
            else:
                stdscr.attron(curses.color_pair(2))
                stdscr.addstr(y, 4, display[:max_x - 10])
                stdscr.attroff(curses.color_pair(2))

        # Footer Status Bar
        footer = " <Select>   <Exit / Return>   <Save Configuration>   <Help> "
        stdscr.attron(curses.color_pair(1))
        stdscr.addstr(max_y - 1, 0, footer.center(max_x - 1)[:max_x - 1])
        stdscr.attroff(curses.color_pair(1))

        stdscr.refresh()

        key = stdscr.getch()

        if key in [curses.KEY_UP, ord('k')]:
            if cursor_stack[-1] > 0:
                cursor_stack[-1] -= 1
        elif key in [curses.KEY_DOWN, ord('j')]:
            if cursor_stack[-1] < len(items) - 1:
                cursor_stack[-1] += 1
        elif key in [curses.KEY_ENTER, 10, 13]:
            sel_item = items[cursor_stack[-1]]
            if "items" in sel_item:
                menu_stack.append(sel_item)
                cursor_stack.append(0)
            elif sel_item.get("type") == "bool":
                name = sel_item["name"]
                cfg[name] = not cfg.get(name, False)
            elif sel_item.get("type") == "radio":
                cfg["CONFIG_ARCH"] = sel_item["val"]
                if sel_item["val"] == "x86_64":
                    cfg["CONFIG_ARCH_X86_64"] = True
                    cfg["CONFIG_ARCH_AARCH64"] = False
                    cfg["CONFIG_ARCH_ARMV8I"] = False
                elif sel_item["val"] == "aarch64":
                    cfg["CONFIG_ARCH_X86_64"] = False
                    cfg["CONFIG_ARCH_AARCH64"] = True
                    cfg["CONFIG_ARCH_ARMV8I"] = False
                elif sel_item["val"] == "armv8i":
                    cfg["CONFIG_ARCH_X86_64"] = False
                    cfg["CONFIG_ARCH_AARCH64"] = False
                    cfg["CONFIG_ARCH_ARMV8I"] = True
        elif key == ord(' '):
            sel_item = items[cursor_stack[-1]]
            if sel_item.get("type") == "bool":
                name = sel_item["name"]
                cfg[name] = not cfg.get(name, False)
            elif sel_item.get("type") == "radio":
                cfg["CONFIG_ARCH"] = sel_item["val"]
                if sel_item["val"] == "x86_64":
                    cfg["CONFIG_ARCH_X86_64"] = True
                    cfg["CONFIG_ARCH_AARCH64"] = False
                    cfg["CONFIG_ARCH_ARMV8I"] = False
                elif sel_item["val"] == "aarch64":
                    cfg["CONFIG_ARCH_X86_64"] = False
                    cfg["CONFIG_ARCH_AARCH64"] = True
                    cfg["CONFIG_ARCH_ARMV8I"] = False
                elif sel_item["val"] == "armv8i":
                    cfg["CONFIG_ARCH_X86_64"] = False
                    cfg["CONFIG_ARCH_AARCH64"] = False
                    cfg["CONFIG_ARCH_ARMV8I"] = True
        elif key in [ord('s'), ord('S')]:
            save_config(cfg)
            stdscr.attron(curses.color_pair(1) | curses.A_BOLD)
            stdscr.addstr(max_y - 2, 4, f" Saved configuration to {CONFIG_FILE} and {AUTOCONF_HEADER}! Press any key... ")
            stdscr.attroff(curses.color_pair(1) | curses.A_BOLD)
            stdscr.refresh()
            stdscr.getch()
        elif key in [ord('q'), ord('Q'), 27]: # Esc or Q
            if menu_stack:
                menu_stack.pop()
                cursor_stack.pop()
            else:
                save_config(cfg)
                break

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
        elif cmd in ["--aarch64", "--armv8", "--armv8i"]:
            defconfig(cmd.lstrip("-"))
            return

    try:
        curses.wrapper(run_tui)
        print(f"*** Configuration saved to {CONFIG_FILE} and {AUTOCONF_HEADER} ***")
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
