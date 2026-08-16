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
[![Networking](https://img.shields.io/badge/network-Intel%20E1000%20Gigabit%20%7C%20IPv4-green.svg?style=flat-square)]()
[![License](https://img.shields.io/badge/license-MIT-lightgrey.svg?style=flat-square)](LICENSE)

**SUB-OS** is an open-source, production-grade 64-bit modular monolithic operating system engineered from the ground up for the `x86_64` architecture. It is built adhering to the internal design principles of modern UNIX/Linux kernels, featuring full bare-metal hardware drivers, physical and dynamic memory management, a virtual file system (VFS), networking protocol stack, cryptographic engine, Linux Security Modules (LSM), asynchronous `io_uring` ringbuffers, hypervisor detection, full-screen visual `nano` editor, and the **LazyBox** multi-call userland environment.

---

## 🏛️ System Architecture

```text
+-------------------------------------------------------------------------+
|                       USERLAND & SYSTEM SUITE                           |
|  Interactive Shell | GNU Nano 2.0.0 | LazyBox Suite (ls, cat, ping, ...) |
+-------------------------------------------------------------------------+
|                       VIRTUAL FILE SYSTEM (VFS)                         |
|   vfs_namei | Root RAMFS | /dev (devfs) | /proc (procfs) | initramfs   |
+-------------------------------------------------------------------------+
|             ASYNC I/O ENGINE & SECURITY FRAMEWORK                       |
|   io_uring (SQE/CQE Rings) | Linux Security Modules (LSM) | Keyring     |
+-------------------------------------------------------------------------+
|                  CORE SERVICES & CRYPTO ENGINE                          |
|  Preemptive Scheduler | Task Control | SHA-256 | MD5 | CRC32 | PRNG     |
+-------------------------------------------------------------------------+
|                       MEMORY MANAGEMENT (MM)                            |
|     Physical Page Frame Allocator (PMM)  |  Kernel Heap (kmalloc/kfree) |
+-------------------------------------------------------------------------+
|                       DEVICE DRIVER TREE & VIRT                         |
|   VGA / TTY1-4   | 16550 UART Serial | PS/2 Keyboard | ATA / IDE Disk   |
|   PCI Enumerator | Intel E1000 NIC   | VirtIO Core   | PC Speaker       |
+-------------------------------------------------------------------------+
|                  HARDWARE ABSTRACTION LAYER (HAL)                       |
|   x86_64 Long Mode | GDT / TSS | IDT / ISRs | Dual 8259A PIC | 8254 PIT |
+-------------------------------------------------------------------------+
```

---

## ⚡ 14-Step Master Boot Pipeline

During boot, the kernel executes a deterministic 14-step master initialization sequence:

| Step | Subsystem | Description |
|:---:|:---|:---|
| **[1/14]** | **64-Bit GDT & TSS** | Configures flat kernel/user code & data segments and Task State Segment with dedicated 64KB kernel stack. |
| **[2/14]** | **IDT & ISR Table** | Populates 256-entry interrupt descriptor table with CPU exception handlers and hardware IRQ routing. |
| **[3/14]** | **Dual 8259A PIC** | Remaps master (IRQ 0-7 -> 0x20-0x27) and slave (IRQ 8-15 -> 0x28-0x2F) interrupt controllers. |
| **[4/14]** | **8254 PIT Timer** | Configures channel 0 for 100 Hz (10ms resolution) driving quantum preemption. |
| **[5/14]** | **PS/2 Keyboard** | Full scancode translation table with circular ringbuffer, Shift, CapsLock, Ctrl keys, and multi-TTY switching. |
| **[6/14]** | **Physical Memory Manager** | 64-bit bitmap frame allocator tracking usable RAM via BIOS E820 map (31,712 free 4KB pages). |
| **[7/14]** | **Kernel Dynamic Heap** | Boundary-tag dynamic heap allocator with 4 MB initial pool, coalescing, and reallocation. |
| **[8/14]** | **ATA / IDE Storage** | 28-bit LBA PIO hard disk controller supporting sector read/write operations on primary master. |
| **[9/14]** | **PCI, VirtIO & Net** | PCI bus enumerator, VirtIO core, and Intel 82540EM Gigabit NIC with circular DMA RX/TX descriptors. |
| **[10/14]** | **Security & Keyring** | Loads X.509 trusted certificate authority keyring and enforces POSIX Capabilities Linux Security Module (LSM). |
| **[11/14]** | **io_uring Subsystem** | Lockless Submission Queue Entry (SQE) & Completion Queue Entry (CQE) asynchronous I/O engine. |
| **[12/14]** | **Virtual File System** | Mounts root in-memory RAMFS, synthetic `/dev` devfs, `/proc` procfs, and unpacks embedded initramfs tree. |
| **[13/14]** | **Scheduler & LazyBox** | Starts round-robin preemptive scheduler and launches LazyBox interactive userland environment. |
| **[14/14]** | **Hardware Interrupts** | Enables processor interrupts (`sti`) and transitions to userland shell. |

---

## 🌟 Subsystems & Linux Directory Structure

### 1. Bootloader & Kernel Core (`arch/x86_64/`, `kernel/`)
- **Two-Stage Custom MBR Bootloader**:
  - `boot/boot.asm`: 512-byte MBR sector loaded at `0x7C00` that loads Stage 2 using BIOS INT 13h extensions.
  - `boot/stage2.asm`: Enables A20 line, queries E820 memory map, builds 4-level PML4 identity paging tables (4GB address space), transitions through 32-bit Protected Mode into 64-bit Long Mode, and loads kernel in 64-sector chunks.
- **Preemptive Multi-Tasking**: Process Control Blocks (PCB), round-robin task scheduler driven by timer ticks, atomic spinlocks, and semaphores.
- **Kernel Logging & Serial Diagnostics**: ANSI color `printk()` supporting loglevels (`KERN_INFO`, `KERN_WARN`, `KERN_ERR`), a 64KB kernel `dmesg` ring buffer, and simultaneous output to COM1 serial port.

### 2. Early Initialization & Versioning (`init/`)
- `init/cmdline.c`: Kernel command line parsing (`root=`, `console=`, `init=`, `quiet`, etc.) and system runlevel management (`RUNLEVEL_SINGLE`, `RUNLEVEL_MULTIUSER`, `RUNLEVEL_GRAPHICAL`).
- `init/version.c`: Version metadata and banner formatting.

### 3. X.509 Cryptographic Keyring (`certs/`)
- `certs/x509.c`: ASN.1 DER parser for X.509 v3 public key certificates (Subject, Issuer, Validity, SHA-256 Fingerprint, RSA/ECDSA trust validation).
- System trusted keyring supporting kernel module and executable signature verification.

### 4. Asynchronous I/O Engine (`io_uring/`)
- `io_uring/io_uring.c`: High-performance non-blocking asynchronous I/O ringbuffers with SQE (Submission Queue Entries) and CQE (Completion Queue Entries) for concurrent disk and network I/O.

### 5. Linux Security Modules & Capabilities (`security/`)
- `security/lsm.c`: Hookable security infrastructure with DAC and MAC permission callbacks.
- `security/capability.c`: POSIX Capabilities model (`CAP_CHOWN`, `CAP_DAC_OVERRIDE`, `CAP_NET_RAW`, `CAP_SYS_ADMIN`, etc.).

### 6. Userspace Initramfs & Payload (`usr/`)
- `usr/initramfs.c`: Populates filesystem tree (`/usr/bin`, `/var/log`, `/etc/passwd`, `/etc/issue`, `/etc/fstab`).

### 7. Virtualization & VirtIO Subsystem (`virt/`)
- `virt/hypervisor.c`: Hypervisor detection via CPUID leaves (QEMU, KVM, VMware, VirtualBox, Hyper-V).
- `virt/virtio.c`: VirtIO core device driver registration and split virtqueue framework.

### 8. Memory Management Subsystem (`mm/`)
- **Physical Memory Manager (`pmm.c`)**: Page frame allocator managing physical RAM using a high-efficiency bitmap.
- **Kernel Dynamic Heap (`kmalloc.c`)**: Provides `kmalloc`, `kzalloc`, `kcalloc`, `krealloc`, and `kfree` with boundary tag headers and adjacent block coalescing.

### 9. Virtual File System (`fs/`)
- **Unified VFS Core (`vfs.c`)**: Posix-like file operations (`open`, `read`, `write`, `close`, `lseek`, `mkdir`, `create`) with hierarchical path resolution (`vfs_namei`) and support for up to 64 file descriptors.
- **Root RAMFS (`ramfs.c`)**: Dynamic in-memory directory and file tree with auto-expanding data buffers.
- **Synthetic Device Filesystem (`devfs.c`)**: Mounted at `/dev`, providing `/dev/null`, `/dev/zero`, `/dev/random`, `/dev/urandom`, and `/dev/tty`.
- **Process Metrics Filesystem (`procfs.c`)**: Mounted at `/proc`, generating dynamic metrics (`/proc/version`, `/proc/meminfo`, `/proc/cpuinfo`, `/proc/uptime`, `/proc/net/dev`).

### 10. Hardware Drivers (`drivers/`)
- **VGA & TTY (`vga.c`, `tty.c`)**: 80x25 text mode driver with 4 Virtual Consoles (TTY1-4 switched via `Alt+F1`–`Alt+F4`) and full ANSI escape sequence parsing.
- **Serial Port (`serial.c`)**: 16550 UART COM1 driver at 115200 baud.
- **PS/2 Keyboard (`keyboard.c`)**: Interrupt-driven scancode decoder with modifier state machine and full Ctrl+key combination mapping.
- **ATA / IDE Disk (`ata.c`)**: 28-bit LBA PIO disk controller.
- **PCI Bus (`pci.c`)**: Enumeration and Base Address Register (BAR) configuration.
- **Intel E1000 Gigabit NIC (`e1000.c`)**: Intel 82540EM Gigabit NIC driver with hardware DMA ringbuffers.
- **PC Speaker (`speaker.c`)**: Programmable timer channel 2 tone generator.

### 11. Cryptography Engine (`crypto/`)
- **SHA-256 (`sha256.c`)**: FIPS 180-4 compliant 256-bit cryptographic digest.
- **MD5 (`md5.c`)**: RFC 1321 compliant 128-bit hash function.
- **CRC32 (`crc32.c`)**: IEEE 802.3 standard cyclic redundancy check.
- **PRNG (`prng.c`)**: Xorshift128+ cryptographic pseudo-random number generator.

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

## 🧰 LazyBox Multi-Call Userland Suite

SUB-OS ships with an extensive multi-call userland binary suite (`lazybox`):

| Category | Commands |
|---|---|
| **Core** | `lazybox`, `echo`, `whoami`, `id`, `date`, `cal` |
| **Filesystem & Editor** | `nano`, `ls`, `cat`, `touch`, `mkdir`, `pwd`, `cd`, `wc`, `head`, `tail`, `stat`, `cp`, `grep`, `hexdump` |
| **Crypto & Security** | `sha256sum`, `md5sum`, `crc32`, `rand`, `certcheck`, `capsh` |
| **Network** | `ifconfig`, `ping`, `arp` |
| **Storage & Devices** | `hdparm`, `lspci`, `speaker` |
| **Virtualization** | `virtinfo`, `io_uring_test` |
| **System Monitoring** | `uname`, `free`, `uptime`, `dmesg`, `ps`, `top`, `sleep`, `reboot`, `poweroff` |
| **Terminal** | `clear`, `neofetch`, `help` |

---

## 🚀 Building & Running

### Prerequisites
- `gcc` (with `-ffreestanding`, `-fno-pie`, `-mcmodel=kernel`) or `x86_64-elf-gcc`
- `nasm` (Netwide Assembler)
- `qemu-system-x86_64` (for hardware virtualization)

### Compilation
```bash
make clean
make
```

### Emulation in QEMU
```bash
# Run with Intel Gigabit NIC, ATA drive, PC Speaker, and Serial Console
qemu-system-x86_64 -drive format=raw,file=SUB-OS.img -nic model=e1000 -serial stdio -audiodev id=snd0,driver=pa -machine pcspk-audiodev=snd0
```
