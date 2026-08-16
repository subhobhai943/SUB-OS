# SUB-OS: 64-Bit Production Modular Monolithic Operating System

```text
   _____ _    _ ____          ____   _____ 
  / ____| |  | |  _ \        / __ \ / ____|
 | (___ | |  | | |_) |______| |  | | (___  
  \___ \| |  | |  _ <|______| |  | |\___ \ 
  ____) | |__| | |_) |      | |__| |____) |
 |_____/ \____/|____/        \____/|_____/ 
```

[![Architecture](https://img.shields.io/badge/arch-x86__64%20Long%20Mode-blue.svg?style=flat-square)]()
[![Kernel](https://img.shields.io/badge/kernel-Modular%20Monolithic-red.svg?style=flat-square)]()
[![Userland](https://img.shields.io/badge/userland-LazyBox%20v2.0.0--pro-orange.svg?style=flat-square)]()
[![Networking](https://img.shields.io/badge/network-Intel%20E1000%20Gigabit%20%7C%20IPv4-green.svg?style=flat-square)]()
[![License](https://img.shields.io/badge/license-MIT-purple.svg?style=flat-square)](LICENSE)

**SUB-OS** is an open-source, production-grade 64-bit modular monolithic operating system engineered from the ground up for the `x86_64` architecture. It is built adhering to the internal design principles of modern UNIX/Linux kernels, featuring full bare-metal hardware drivers, physical and dynamic memory management, a virtual file system (VFS), networking protocol stack, cryptographic engine, and the **LazyBox** multi-call userland environment.

---

## 🏛️ System Architecture

```text
+-------------------------------------------------------------------------+
|                       USERLAND & SYSTEM SUITE                           |
|   Interactive Shell  |  LazyBox Applets (ls, cat, ping, sha256sum, ...) |
+-------------------------------------------------------------------------+
|                       VIRTUAL FILE SYSTEM (VFS)                         |
|      vfs_namei  |  Root RAMFS  |  /dev (devfs)  |  /proc (procfs)       |
+-------------------------------------------------------------------------+
|                  CORE SERVICES & CRYPTO ENGINE                          |
|  Preemptive Scheduler | Task Control (PCB) | SHA-256 | MD5 | CRC32|PRNG |
+-------------------------------------------------------------------------+
|                       MEMORY MANAGEMENT (MM)                            |
|     Physical Page Frame Allocator (PMM)  |  Kernel Heap (kmalloc/kfree) |
+-------------------------------------------------------------------------+
|                       DEVICE DRIVER TREE                                |
|   VGA / TTY1-4   | 16550 UART Serial | PS/2 Keyboard | ATA / IDE Disk   |
|   PCI Enumerator | Intel E1000 NIC   | Net (ARP/IPv4/ICMP) | PC Speaker |
+-------------------------------------------------------------------------+
|                  HARDWARE ABSTRACTION LAYER (HAL)                       |
|   x86_64 Long Mode | GDT / TSS | IDT / ISRs | Dual 8259A PIC | 8254 PIT |
+-------------------------------------------------------------------------+
```

---

## ⚡ 11-Step Master Boot Pipeline

During boot, the kernel executes a deterministic 11-step master initialization sequence:

| Step | Subsystem | Description |
|:---:|:---|:---|
| **[1/11]** | **64-Bit GDT & TSS** | Configures flat kernel/user code & data segments and Task State Segment with dedicated 64KB kernel stack. |
| **[2/11]** | **IDT & ISR Table** | Populates 256-entry interrupt descriptor table with CPU exception handlers and hardware IRQ routing. |
| **[3/11]** | **Dual 8259A PIC** | Remaps master (IRQ 0-7 -> 0x20-0x27) and slave (IRQ 8-15 -> 0x28-0x2F) interrupt controllers. |
| **[4/11]** | **8254 PIT Timer** | Configures channel 0 for 100 Hz (10ms resolution) driving quantum preemption. |
| **[5/11]** | **PS/2 Keyboard** | Full scancode translation table with circular ringbuffer, Shift, CapsLock, and multi-TTY switching. |
| **[6/11]** | **Physical Memory Manager** | 64-bit bitmap frame allocator tracking usable RAM via BIOS E820 map (31,712 free 4KB pages). |
| **[7/11]** | **Kernel Dynamic Heap** | Boundary-tag dynamic heap allocator with 4 MB initial pool, coalescing, and reallocation. |
| **[8/11]** | **ATA / IDE Storage** | 28-bit LBA PIO hard disk controller supporting sector read/write operations on primary master. |
| **[9/11]** | **PCI & Intel E1000 NIC** | PCI bus enumerator, BAR configuration, and Intel 82540EM Gigabit NIC with circular DMA RX/TX descriptors. |
| **[10/11]** | **Virtual File System** | Mounts root in-memory RAMFS, synthetic `/dev` devfs, and dynamic `/proc` procfs. |
| **[11/11]** | **Scheduler & LazyBox** | Starts round-robin preemptive scheduler and launches LazyBox interactive userland environment. |

---

## 🌟 Subsystems Overview

### 1. Bootloader & Kernel Core (`arch/x86_64/`, `kernel/`)
- **Two-Stage Custom MBR Bootloader**:
  - `stage1.asm`: 512-byte MBR sector loaded at `0x7C00` that loads Stage 2 using BIOS INT 13h extensions.
  - `stage2.asm`: Enables A20 line, queries E820 memory map, builds 4-level PML4 identity paging tables (4GB address space), transitions through 32-bit Protected Mode into 64-bit Long Mode, and loads kernel in 64-sector chunks.
- **Preemptive Multi-Tasking**: Process Control Blocks (PCB), round-robin task scheduler driven by timer ticks, atomic spinlocks, and semaphores.
- **Kernel Logging & Serial Diagnostics**: ANSI color `printk()` supporting loglevels (`KERN_INFO`, `KERN_WARN`, `KERN_ERR`), a 64KB kernel `dmesg` ring buffer, and simultaneous output to COM1 serial port.

### 2. Memory Management Subsystem (`mm/`)
- **Physical Memory Manager (`pmm.c`)**: Page frame allocator managing physical RAM using a high-efficiency bitmap.
- **Kernel Dynamic Heap (`kmalloc.c`)**: Provides `kmalloc`, `kzalloc`, `kcalloc`, `krealloc`, and `kfree` with boundary tag headers and adjacent block coalescing.

### 3. Virtual File System (`fs/`)
- **Unified VFS Core (`vfs.c`)**: Posix-like file operations (`open`, `read`, `write`, `close`, `lseek`, `mkdir`, `create`) with hierarchical path resolution (`vfs_namei`) and support for up to 64 file descriptors.
- **Root RAMFS (`ramfs.c`)**: Dynamic in-memory directory and file tree with auto-expanding data buffers.
- **Synthetic Device Filesystem (`devfs.c`)**: Mounted at `/dev`, providing `/dev/null`, `/dev/zero`, `/dev/random`, `/dev/urandom`, and `/dev/tty`.
- **Dynamic Information Filesystem (`procfs.c`)**: Mounted at `/proc`, providing live virtual metrics via `/proc/version`, `/proc/meminfo`, and `/proc/cpuinfo`.

### 4. Device Drivers (`drivers/`)
- **Virtual Terminals (`tty.c`)**: Four independent virtual terminals (`tty1`-`tty4`) with ANSI escape code processing, cursor positioning, and `Alt+F1`-`Alt+F4` switching.
- **Storage Controller (`ata.c`)**: ATA PIO mode driver for 28-bit LBA hard disk operations.
- **PCI Bus Enumerator (`pci.c`)**: Discovers hardware devices across buses, slots, and functions, reading vendor/device IDs and mapping BARs.
- **Intel E1000 Gigabit NIC (`e1000.c`)**: Configures receive/transmit circular DMA rings, link state auto-negotiation, and hardware packet dispatch.
- **Audio (`speaker.c`)**: Square-wave audio generation using PIT Channel 2.

### 5. Network Stack (`net/core/`)
- Layer 2 Ethernet frame encapsulation and MAC parsing.
- ARP request/reply generation and dynamic ARP caching.
- IPv4 packet validation, header checksumming, and gateway routing.
- ICMP Echo Request/Reply client and server (`ping`).

### 6. Cryptography Engine (`crypto/`)
- **SHA-256**: FIPS 180-4 compliant 256-bit message digest.
- **MD5**: RFC 1321 compliant 128-bit hashing algorithm.
- **CRC32**: IEEE 802.3 standard cyclic redundancy check.
- **PRNG**: Xorshift128+ cryptographic pseudorandom generator.

---

## 💻 LazyBox Multi-Call Userland Suite

**LazyBox** (`userland/lazybox/`) provides a single-binary multi-call command suite inspired by BusyBox:

| Category | Applets |
|---|---|
| **Filesystem** | `ls`, `cat`, `touch`, `mkdir`, `wc` |
| **Crypto** | `sha256sum`, `md5sum`, `crc32` |
| **Network** | `ifconfig`, `ping`, `arp` |
| **Storage** | `hdparm` |
| **System** | `uname`, `free`, `uptime`, `dmesg`, `sleep` |
| **Terminal** | `clear`, `neofetch` |
| **Core** | `lazybox`, `echo`, `calc` |

---

## 📺 Live Output Demonstrations

### 1. `neofetch`
```text
sub-os> neofetch
   _____ _    _ ____     OS:      SUB-OS v0.2.0-lts (Modular Monolithic)
  / ____| |  | |  _ \    Arch:    x86_64 (64-Bit Long Mode)
 | (___ | |  | | |_) |   Kernel:  Modular Drivers + VFS + Crypto + Net
  \___ \| |  | |  _ <    CPU:     QEMU Virtual CPU (AuthenticAMD)
  ____) | |__| | |_) |   Memory:  8 MB / 127 MB (Free: 119 MB)
 |_____/ \____/|____/    Heap:    110 KB used / 4095 KB total
                         Uptime:  00:00:02
                         TTY:     tty1 (Alt+F1-F4 to switch)
```

### 2. Network ICMP Ping (`ping 10.0.2.2`)
```text
sub-os> ping 10.0.2.2
PING 10.0.2.2 (56 data bytes):
64 bytes from 10.0.2.2: icmp_seq=1 ttl=64 time=0 ms
64 bytes from 10.0.2.2: icmp_seq=2 ttl=64 time=0 ms
```

### 3. FIPS SHA-256 Cryptographic Hash (`sha256sum /readme.txt`)
```text
sub-os> sha256sum /readme.txt
4e0391a7fc1a293893645d3d40f103f2eb28d442d0b8815bf86473d0bfd8e7fd  /readme.txt
```

### 4. Dynamic Process Information (`cat /proc/meminfo`)
```text
sub-os> cat /proc/meminfo
MemTotal:         130944 kB
MemFree:          122752 kB
MemAvailable:     122752 kB
Buffers:               0 kB
Cached:              110 kB
HeapTotal:          4096 kB
HeapUsed:            110 kB
HeapFree:           3985 kB
```

---

## 🛠️ Build and Emulation

### Prerequisites
- **Assembler**: `nasm` (>= 2.15)
- **Cross-Compiler**: `x86_64-elf-gcc` (>= 13.0) & `x86_64-elf-ld`
- **Emulator**: `qemu-system-x86_64`
- **Build System**: GNU `make`

### Building the OS
```bash
# Set cross-compiler path
export PATH="/usr/local/cross/bin:$PATH"

# Build kernel image (SUB-OS.img)
make

# Run in QEMU with Intel E1000 NIC and ATA drive
make run

# Clean build artifacts
make clean
```

---

## ⌨️ Terminal Keybindings

| Keybinding | Action |
|---|---|
| `Alt + F1` | Switch to **TTY1** (Main Interactive Shell) |
| `Alt + F2` | Switch to **TTY2** (Secondary Virtual Terminal) |
| `Alt + F3` | Switch to **TTY3** (System Diagnostics Terminal) |
| `Alt + F4` | Switch to **TTY4** (Background Task Terminal) |
| `Up / Down` | Command History Navigation |
| `Left / Right` | Interactive In-Line Cursor Navigation |
| `Backspace` | Character Erasure with Terminal Synchronisation |
| `Tab` | Command & Path Auto-Completion |

---

## 📁 Repository Structure

```text
SUB-OS/
├── arch/x86_64/            # CPU, Paging, GDT/TSS, IDT/ISR, PIC, PIT, CPUID
├── boot/                   # Stage 1 MBR & Stage 2 64-bit Switch Bootloader
├── crypto/                 # SHA-256, MD5, CRC32, CSPRNG
├── drivers/                # Character, Block, PCI, Network (E1000), Sound
├── fs/                     # VFS Core, RAMFS, devfs, procfs
├── include/                # Complete Modular Kernel C Header Files
├── kernel/                 # Main, Scheduler, Tasks, Synchronization, printk, Panic
├── lib/                    # vsprintf, Strings, Bitmap
├── mm/                     # Physical Page Allocator (PMM) & Heap (kmalloc)
├── net/core/               # Ethernet, ARP, IPv4, ICMP Protocol Stack
├── tools/                  # Automated Setup and Cross-Compiler Scripts
├── userland/lazybox/       # LazyBox Multi-Call Binary & Interactive Shell
├── Makefile                # Complete Build Orchestrator
├── linker.ld               # Kernel 64-bit Linker Script
└── README.md               # Documentation & Specifications
```

---

## 📄 License

This project is licensed under the terms of the [MIT License](LICENSE).
