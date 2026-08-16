# SUB-OS 64-Bit Production Monolithic Operating System

[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Architecture](https://img.shields.io/badge/arch-x86__64%20Long%20Mode-blue.svg)]()
[![Userland](https://img.shields.io/badge/userland-LazyBox%20v2.0.0--pro-orange.svg)]()
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

**SUB-OS** is a modular monolithic 64-bit x86_64 operating system designed and engineered from scratch following modern Linux kernel architectural patterns and incorporating the **LazyBox** multi-call userland binary suite.

---

## 🌟 Key Features

- **64-Bit Long Mode Bare-Metal Core**: Custom two-stage MBR bootloader with BIOS E820 memory mapping and 4GB 4-level PML4 identity paging.
- **Modular Monolithic Linux Layout**: Clean directory structure (`arch/`, `kernel/`, `mm/`, `drivers/`, `fs/`, `net/`, `crypto/`, `lib/`, `userland/`).
- **Preemptive Multitasking Scheduler**: Round-Robin scheduling driven by 8254 PIT (100 Hz, 10ms resolution) with atomic spinlocks & semaphores.
- **Physical & Dynamic Memory Management**: 64-bit Page Frame Allocator (4 KB bitmap) and Kernel Dynamic Heap (4 MB pool) with boundary tag splitting and coalescing.
- **Virtual File System (VFS)**: Path resolver (`vfs_namei`), file descriptors (64 max), in-memory RAMFS, synthetic `/dev` device filesystem, and `/proc` dynamic system metrics filesystem.
- **Hardware Drivers**:
  - **Char**: VGA Text Mode, Virtual Terminals TTY1-4 with ANSI escape parsing, 16550 UART Serial (COM1), PS/2 Keyboard with circular buffer.
  - **Block**: ATA / IDE Hard Disk PIO driver (28-bit LBA).
  - **PCI**: PCI Bus Enumeration & MMIO BAR configuration.
  - **Network**: Intel 82540EM/82545EM Gigabit Ethernet NIC with circular DMA RX/TX rings.
  - **Sound**: PC Speaker square-wave tone generator.
- **Network Protocol Stack**: Ethernet encapsulation, ARP resolution table, IPv4 routing/checksums, and ICMP Echo client/server (`ping`).
- **Kernel Cryptographic Engine**: FIPS 180-4 SHA-256, RFC 1321 MD5, IEEE 802.3 CRC32, and Xorshift128+ CSPRNG.
- **LazyBox Multi-Call Userland Suite**: 42 built-in applets with history, arrow-key line editing, and ANSI color support.

---

## 📁 Source Tree

```
├── arch/x86_64/            # Hardware-specific CPU, paging, and entry code
│   ├── boot/               # 64-bit kernel entry point (entry.asm)
│   ├── cpu/                # GDT/TSS, IDT (256 gates), Dual 8259A PIC, 8254 PIT, CPUID
│   └── mm/                 # 4-Level PML4 Paging & TLB management
├── boot/                   # Custom two-stage MBR bootloader
│   ├── boot.asm            # Stage 1: 512-byte MBR loader (INT 13h LBA/CHS)
│   └── stage2.asm          # Stage 2: A20, E820 Memory Map, 4GB Identity Paging, Long Mode switch
├── kernel/                 # Core Monolithic Kernel Engine
│   ├── main.c              # 11-step master initialization sequence
│   ├── printk.c            # ANSI loglevel printk with 64KB dmesg ringbuffer & COM1 serial port output
│   ├── panic.c             # Kernel panic screen with 64-bit register dump
│   ├── task.c              # Task Control Blocks (PCB) & Process Management
│   ├── sched.c             # Preemptive Round-Robin Multi-Tasking Scheduler
│   └── sync.c              # Atomic spinlocks and semaphores
├── mm/                     # Memory Management Subsystem
│   ├── pmm.c               # 64-bit Physical Page Frame Allocator (4 KB bitmap)
│   └── kmalloc.c           # Kernel Dynamic Heap Allocator (4 MB initial pool)
├── drivers/                # Device Driver Tree
│   ├── char/               # VGA text driver, Virtual Terminals TTY1-4, 16550 UART COM1, PS/2 Keyboard
│   ├── block/              # ATA / IDE Hard Disk PIO driver (28-bit LBA)
│   ├── pci/                # PCI Bus Enumerator & BAR mapping
│   ├── net/                # Intel 82540EM/82545EM Gigabit Ethernet NIC Driver (Circular DMA Rings)
│   └── sound/              # PC Speaker tone generator & PIT Channel 2
├── fs/                     # Virtual File System (VFS)
│   ├── vfs.c               # VFS core, path resolver (vfs_namei), file descriptor table (64 max)
│   ├── ramfs.c             # In-memory RAM filesystem
│   ├── devfs.c             # Synthetic Device Filesystem (/dev/null, /dev/zero, /dev/random, /dev/tty)
│   └── procfs.c            # Dynamic Information Filesystem (/proc/version, /proc/meminfo, /proc/cpuinfo)
├── crypto/                 # Kernel Cryptographic Engine
│   ├── sha256.c            # FIPS 180-4 Standard SHA-256 Digest
│   ├── md5.c               # RFC 1321 MD5 Message-Digest
│   ├── crc32.c             # IEEE 802.3 32-bit Cyclic Redundancy Check
│   └── prng.c              # Xorshift128+ High-Performance CSPRNG
├── net/core/               # Network Protocol Stack
│   └── net.c               # Ethernet encapsulation, ARP Table, IPv4 routing, ICMP Echo ping client/server
├── lib/                    # Standard Kernel Runtime Library
│   ├── string.c            # String & memory manipulation
│   ├── vsprintf.c          # vsnprintf with width, alignment, and formatting
│   └── bitmap.c            # Bit manipulation utilities
└── userland/lazybox/       # Userland Shell & Multi-Call Suite
    ├── lazybox.c           # Multi-call binary dispatcher with 42 applets
    └── shell.c             # Interactive terminal shell (history, tab-completion, ANSI color)
```

---

## ⚡ 11-Step Master Boot Pipeline

1. **[1/11] GDT & TSS**: 64-bit flat segment selectors + Task State Segment with dedicated 64KB kernel stack.
2. **[2/11] IDT & ISRs**: 256-entry Interrupt Gate Table with CPU exception and hardware IRQ handlers.
3. **[3/11] Dual 8259A PIC**: Remapped hardware IRQs 0-15 to vectors 32-47 with cascade handling.
4. **[4/11] 8254 PIT Timer**: Configured for 100 Hz (10ms resolution) driving preemptive multitasking.
5. **[5/11] PS/2 Keyboard**: Full scancode translation table with Shift, CapsLock, and circular buffer.
6. **[6/11] Physical Memory Manager**: 64-bit frame allocator tracking 128 MB RAM (31,712 free pages).
7. **[7/11] Kernel Dynamic Heap**: 4 MB initial pool with boundary tag headers, coalescing, and reallocation.
8. **[8/11] ATA / IDE Hard Disk**: 28-bit LBA PIO controller supporting read/write operations.
9. **[9/11] PCI & Intel E1000 NIC**: Intel Gigabit Ethernet controller (`52:54:00:12:34:56`), DMA RX/TX rings.
10. **[10/11] Virtual File System**: Root RAMFS, `/dev` synthetic device filesystem, `/proc` dynamic info filesystem.
11. **[11/11] LazyBox & Scheduler**: Preemptive scheduler initialization and interactive multi-call binary userland.

---

## 🛠️ Build and Run

### Prerequisites
- `nasm` (>= 2.15)
- `x86_64-elf-gcc` (>= 13.0) & `x86_64-elf-ld`
- `qemu-system-x86_64`
- `make`

### Commands
```bash
# Build the 1.5MB bootable disk image
make

# Launch in QEMU with Intel E1000 Gigabit NIC
make run

# Clean build artifacts
make clean
```

---

## 💻 LazyBox Applet Reference

```
  [Filesystem]
    ls           cat          touch        mkdir        wc
  [Crypto]
    md5sum       sha256sum    crc32
  [Network]
    ifconfig     ping         arp
  [Storage]
    hdparm
  [System]
    uname        free         uptime       dmesg        sleep
  [Terminal]
    clear
  [Core]
    lazybox      echo         neofetch
```

---

## 📄 License
This project is open-source software licensed under the [MIT License](LICENSE).
