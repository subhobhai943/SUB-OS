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
[![Version](https://img.shields.io/badge/version-v0.2.0--lts%20(Titan)-purple.svg?style=flat-square)]()
[![Userland](https://img.shields.io/badge/userland-LazyBox%20v2.0.0--pro%20%2B%20Nano-orange.svg?style=flat-square)]()
[![Networking](https://img.shields.io/badge/network-Intel%20E1000%20Gigabit%20%7C%20IPv4%20%7C%20BSD%20Sockets-green.svg?style=flat-square)]()
[![License](https://img.shields.io/badge/license-MIT-lightgrey.svg?style=flat-square)](LICENSE)

**SUB-OS** is an open-source, production-grade 64-bit modular monolithic operating system engineered from the ground up for the `x86_64` architecture. Built adhering to the internal design principles of modern UNIX/Linux kernels, SUB-OS features a full bare-metal hardware driver tree, physical and dynamic memory management, a virtual file system (VFS) with FAT32, ext2, sysfs, and devfs, full L2-L4 networking protocol stack with BSD Sockets, cryptographic engine, Linux Security Modules (LSM), asynchronous `io_uring` ringbuffers, Inter-Process Communication (IPC) engine, Text-to-Speech (TTS) voice synthesizer, AC'97 sound architecture, full-screen visual `nano` editor, and the **LazyBox** multi-call userland environment with 50+ Linux utilities.

---

## 🏛️ Comprehensive System Architecture

```text
+------------------------------------------------------------------------------------+
|                             USERLAND & SYSTEM SUITE                                |
|  Interactive Shell | GNU Nano 2.0.0 | LazyBox Multi-Call Suite (50+ Coreutils)     |
+------------------------------------------------------------------------------------+
|                         VIRTUAL FILE SYSTEM LAYER (VFS)                            |
|  vfs_namei | RAMFS (/) | /dev (devfs) | /proc (procfs) | /sys (sysfs) | FAT32 | ext2|
+------------------------------------------------------------------------------------+
|              INTER-PROCESS COMMUNICATION & ASYNC I/O ENGINE                        |
|  Anonymous Pipes | Message Queues | Shared Memory | Semaphores | io_uring Rings    |
+------------------------------------------------------------------------------------+
|                   CORE KERNEL SERVICES & SECURITY MODULE (LSM)                     |
|  Preemptive Scheduler | Task Control | POSIX Signals | Fast Syscalls | Workqueues  |
|  High-Res Timer Wheel | Dynamic Module Loader | X.509 Keyring | POSIX Capabilities |
+------------------------------------------------------------------------------------+
|                    NETWORK PROTOCOL STACK (L2 -> L4) & SOUND                       |
|  Ethernet L2 | ARP | IPv4 | ICMP | UDP | TCP | DHCP | DNS | BSD Sockets            |
|  ALSA Sound Architecture | AC'97 Audio | PCM Streams | Formant TTS Synthesizer     |
+------------------------------------------------------------------------------------+
|                             MEMORY MANAGEMENT (MM)                                 |
|     Physical Page Frame Allocator (PMM)  |  Kernel Dynamic Heap (kmalloc/kfree)    |
+------------------------------------------------------------------------------------+
|                             DEVICE DRIVER TREE & VIRT                              |
|   VGA / TTY1-4 | 16550 UART Serial | PS/2 Keyboard | PS/2 Mouse | ATA / IDE Disk   |
|   PCI Enumerator | Intel E1000 NIC | VESA FB | CMOS RTC | ACPI Power | USB Core    |
+------------------------------------------------------------------------------------+
|                        HARDWARE ABSTRACTION LAYER (HAL)                            |
|   x86_64 Long Mode | GDT / TSS | IDT / ISRs | Dual 8259A PIC | 8254 PIT Timer      |
+------------------------------------------------------------------------------------+
```

---

## ⚡ 18-Step Master Boot Pipeline

During boot, the kernel executes a deterministic 18-step master initialization sequence:

| Step | Subsystem | Description |
|:---:|:---|:---|
| **[1/18]** | **64-Bit GDT & TSS** | Configures flat kernel/user code & data segments and Task State Segment with dedicated 64KB kernel stack. |
| **[2/18]** | **IDT & ISR Table** | Populates 256-entry interrupt descriptor table with CPU exception handlers and hardware IRQ routing. |
| **[3/18]** | **Dual 8259A PIC** | Remaps master (IRQ 0-7 -> 0x20-0x27) and slave (IRQ 8-15 -> 0x28-0x2F) interrupt controllers. |
| **[4/18]** | **PIT & Timer Wheel** | Configures 8254 PIT channel 0 for 100 Hz (10ms resolution) driving quantum preemption & timer wheel. |
| **[5/18]** | **Input & ACPI** | PS/2 Keyboard scancode decoder, PS/2 Mouse packet tracker, and ACPI power management subsystem. |
| **[6/18]** | **PMM & Dynamic Heap** | 64-bit bitmap frame allocator tracking usable RAM via BIOS E820 map and 4 MB dynamic kernel heap. |
| **[7/18]** | **Block Layer & ATA** | Generic Block Layer with request queues, 'noop' elevator scheduler, and 28-bit LBA ATA driver. |
| **[8/18]** | **PCI, VirtIO, FB & USB** | PCI bus enumerator, VirtIO core, Linear 32-bit Framebuffer graphics, and USB host controller. |
| **[9/18]** | **L2-L4 Network Stack** | Intel E1000 NIC, ARP cache, IPv4 routing, ICMP Ping, UDP, TCP state machine, DHCP & DNS. |
| **[10/18]** | **Sound Architecture** | Audio core driver, AC'97 soundcard, PCM stream buffer, and Formant Text-to-Speech synthesizer. |
| **[11/18]** | **Keyring & Security** | Loads X.509 trusted certificate authority keyring and enforces POSIX Capabilities Linux Security Module. |
| **[12/18]** | **IPC Engine** | Anonymous Pipes, System V & POSIX Message Queues, Shared Memory segments, and Semaphore sets. |
| **[13/18]** | **io_uring Subsystem** | Lockless Submission Queue Entry (SQE) & Completion Queue Entry (CQE) asynchronous I/O engine. |
| **[14/18]** | **Multi-VFS Mount** | Root RAMFS, synthetic `/dev` devfs, `/proc` procfs, `/sys` sysfs, FAT32 driver, ext2 driver & initramfs. |
| **[15/18]** | **Kernel Core Services** | POSIX Signal manager (32 signals), Async Workqueues, 64-bit Fast Syscalls, and Dynamic Module Loader. |
| **[16/18]** | **Scheduler & LazyBox** | Starts round-robin preemptive scheduler and launches LazyBox interactive userland environment. |
| **[17/18]** | **Hardware Interrupts** | Enables processor interrupts (`sti`) and transitions to userland shell. |
| **[18/18]** | **Interactive Shell** | Launches multi-TTY interactive terminal console on TTY1. |

---

## 🌟 Subsystems & Codebase Architecture

```text
SUB-OS/
├── arch/x86_64/         # Long Mode bootstrap, GDT, IDT, ISRs, PIC, PIT, CPUID, Paging
├── block/               # Block Layer Core, Request Queues, Elevator I/O Schedulers
├── boot/                # Stage 1 MBR Bootloader & Stage 2 64-Bit Protected Mode Switcher
├── certs/               # X.509 ASN.1 DER Parser, Public Key Verification & Root Keyring
├── crypto/              # FIPS SHA-256, RFC 1321 MD5, IEEE 802.3 CRC32, Xorshift128+ PRNG
├── drivers/
│   ├── block/           # ATA / IDE Hard Disk PIO Driver (28-bit LBA)
│   ├── char/            # VGA Text, TTY1-4 with ANSI sequences, 16550 UART Serial, PS/2 Keyboard
│   ├── input/           # PS/2 Mouse Driver with packet tracking
│   ├── net/             # Intel 82540EM Gigabit Ethernet NIC with DMA Descriptors
│   ├── pci/             # PCI Bus Enumeration & BAR Configuration
│   ├── power/           # ACPI S5 Soft-Off Power Management & Reset
│   ├── rtc/             # CMOS Real-Time Clock Hardware Driver (BCD conversion)
│   ├── sound/           # PC Speaker Tone Generator
│   ├── usb/             # USB Host Controller Abstraction
│   └── video/           # 32-bit Linear Framebuffer Graphics Engine
├── fs/                  # VFS Layer, RAMFS, devfs (/dev), procfs (/proc), sysfs (/sys), FAT32, ext2
├── include/             # Modular Subsystem Header Files
├── init/                # Early Initialization, Boot Command Line Parser, Versioning & Runlevels
├── io_uring/            # Asynchronous I/O Engine with SQE & CQE Lockless Ringbuffers
├── ipc/                 # Anonymous Pipes, Message Queues, Shared Memory, Counting Semaphores
├── kernel/              # Task Scheduler, Mutexes, Signals, Syscalls, Timers, Workqueues, Modules
├── lib/                 # Standard C Library (string, sprintf, bitmap, list)
├── mm/                  # Physical Page Frame Allocator (PMM) & Dynamic Heap (kmalloc)
├── net/                 # Ethernet, ARP, IPv4, ICMP, UDP, TCP, DHCP, DNS, BSD Sockets
├── security/            # Linux Security Modules (LSM) & POSIX Capabilities
├── sound/               # ALSA Sound Core, AC'97 Driver, PCM Audio & Text-to-Speech (TTS) Engine
├── usr/                 # Embedded Initramfs Payload Tree (/etc/passwd, /etc/fstab, /usr/bin)
└── userland/            # Full-Screen GNU Nano Editor & 50+ LazyBox Core Utilities
```

---

## 📝 Full-Screen Visual Nano Editor

SUB-OS includes a full-screen interactive ANSI text editor (`nano`):

```text
  GNU nano 2.0.0             File: /home/user/notes.txt               [Modified]
Hello from nano editor in SUB-OS!
This is a production-level modular operating system.
You can write, edit, search, and save files directly to the VFS.
~
~
~
~
~
~
~
~
~
~
~
[ Wrote 148 bytes to /home/user/notes.txt ]
^G Get Help  ^O WriteOut  ^W Where Is  ^K Cut Line  ^U Paste  ^X Exit
```

### Keybindings:
- **`^O` (Ctrl+O)**: WriteOut / Save file to Virtual File System.
- **`^X` (Ctrl+X)**: Exit Nano and return to shell.
- **`^K` (Ctrl+K)**: Cut current line to cut-buffer.
- **`^U` (Ctrl+U)**: Uncut / Paste line at cursor position.
- **`^G` (Ctrl+G)**: Display help cheat sheet.
- **`Arrow Keys` / `Home` / `End` / `PgUp` / `PgDn`**: Smooth cursor navigation.

---

## 🧰 LazyBox Multi-Call Userland Suite (50+ Utilities)

| Category | Commands Included |
|---|---|
| **Core** | `lazybox`, `echo`, `whoami`, `id`, `date` (CMOS RTC), `cal` |
| **Filesystem & Editor** | `nano`, `ls`, `cat`, `touch`, `mkdir`, `pwd`, `cd`, `wc`, `head`, `tail`, `stat`, `cp`, `grep`, `hexdump` |
| **Sound & Voice** | `tts` (Phonetic Formant Voice Synthesizer), `alsamixer` |
| **Kernel & Modules** | `lsmod`, `insmod`, `rmmod` |
| **Crypto & Security** | `sha256sum`, `md5sum`, `crc32`, `rand`, `certcheck`, `capsh`, `ipcs` |
| **Network** | `ifconfig`, `ping`, `arp`, `dhclient`, `nslookup` |
| **Storage & Devices** | `hdparm`, `lspci`, `speaker`, `mouse` |
| **Virtualization & Async**| `virtinfo`, `io_uring_test` |
| **System Monitoring** | `uname`, `free`, `uptime`, `dmesg`, `ps`, `top`, `sleep`, `reboot`, `poweroff` |
| **Terminal** | `clear`, `neofetch`, `help` |

---

## 🚀 Building & Running

### Compilation
```bash
make clean
make
```

### Emulation in QEMU
```bash
qemu-system-x86_64 -drive format=raw,file=SUB-OS.img -nic model=e1000 -serial stdio -audiodev id=snd0,driver=pa -machine pcspk-audiodev=snd0
```
