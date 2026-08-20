# SUB-OS: Modular Monolithic Linux-Compatible Operating System

```text
   _____ _    _ ____          ____   _____ 
  / ____| |  | |  _ \        / __ \ / ____|
 | (___ | |  | | |_) |______| |  | | (___  
  \___ \| |  | |  _ <|______| |  | |\___ \ 
  ____) | |__| | |_) |      | |__| |____) |
 |_____/ \____/|____/        \____/|_____/ 
```

[![Architectures](https://img.shields.io/badge/arch-x86__64%20%7C%20aarch64%20%7C%20armv8i-blue.svg?style=flat-square)]()
[![Kernel](https://img.shields.io/badge/kernel-Modular%20Monolithic-red.svg?style=flat-square)]()
[![C++ Layer](https://img.shields.io/badge/c%2B%2B-Freestanding%20C%2B%2B17%20OOP-blue.svg?style=flat-square)]()
[![Rust Layer](https://img.shields.io/badge/rust-Rust--for--SUB--OS%20no__std-orange.svg?style=flat-square)]()
[![SUB Language](https://img.shields.io/badge/sub--lang-Native%20%2B%20In--Kernel%20VM-blueviolet.svg?style=flat-square)]()
[![Configuration](https://img.shields.io/badge/config-Linux%20Kconfig%20lxdialog%20TUI-cyan.svg?style=flat-square)]()
[![Version](https://img.shields.io/badge/version-v0.2.0--lts-purple.svg?style=flat-square)]()
[![Release](https://img.shields.io/badge/release-v0.0.1--beta-brightgreen.svg?style=flat-square)](https://github.com/subhobhai943/SUB-OS/releases/tag/v0.0.1-beta)
[![Userland](https://img.shields.io/badge/userland-LazyBox%20%2B%20Nano%20%2B%20Sh-orange.svg?style=flat-square)]()
[![Networking](https://img.shields.io/badge/network-TCP%2FIP%20%7C%20SSHD%20%7C%20HTTPD%20%7C%20NetFilter-green.svg?style=flat-square)]()
[![License](https://img.shields.io/badge/license-MIT-lightgrey.svg?style=flat-square)](LICENSE)

**SUB-OS** is a multi-architecture, production-grade modular monolithic operating system kernel engineered in **C, C++17, Assembly, freestanding bare-metal Rust (`no_std`), and the custom SUB Language (`.sb`)**. Built for high performance, memory safety, modularity, and Unix/Linux compatibility, SUB-OS supports **64-bit x86 (`x86_64`)**, **64-bit ARM (`aarch64`)**, and **32-bit ARM (`armv8i` / AArch32)** targets.

---

## 🌟 Key Architecture & Capabilities

- ⚡ **Freestanding C++17 OOP Kernel Engine (`kernel/cpp/`)**:
  - **Bare-Metal C++ Runtime**: `operator new`/`delete`, sized deallocation, placement `new`, pure virtual handlers (`__cxa_pure_virtual`), and `.init_array` global constructor dispatcher.
  - **Object-Oriented Subsystems**: Polymorphic `AbstractKernelService` hierarchy with runtime virtual method dispatch, RAII, dynamic memory management, and template-based fixed ring buffers (`FixedRingBuffer`).
  - **Interactive C++ Commands**: `cppinfo` (Kernel telemetry & status) and `cpptest` (Polymorphic dispatch & dynamic allocation verifier).
- 🐍 **Interactive In-Kernel Snake Game (`userland/lazybox/snake.c`)**:
  - Full-featured ANSI terminal game featuring real-time keyboard control (`WASD` and Arrow keys), dynamic food generation, score counter, body growth, wall/self collision detection, and autonomous simulation mode (`snake --demo`).
- 🔮 **SUB Programming Language Kernel Signature & In-Kernel VM (`sub/` & `kernel/sub/`)**:
  - **Native `.sb` Kernel Modules**: Key subsystems written directly in the custom SUB language:
    - [`sub/signature.sb`](file:///home/subhobhai943/Github/SUB-OS/sub/signature.sb): Kernel identity, OS signature, and author credits.
    - [`sub/power_governor.sb`](file:///home/subhobhai943/Github/SUB-OS/sub/power_governor.sb): Dynamic CPU P-state thermal scaling and frequency/voltage calculations.
    - [`sub/benchmark.sb`](file:///home/subhobhai943/Github/SUB-OS/sub/benchmark.sb): Recursive Fibonacci and integer matrix benchmark algorithms.
    - [`sub/easter_egg.sb`](file:///home/subhobhai943/Github/SUB-OS/sub/easter_egg.sb): Interactive kernel quotes and easter eggs.
  - **In-Kernel SUB Virtual Machine & AST Interpreter (`subi`)**: Run `.sb` scripts or inline expressions live from the shell (`subi file.sb` or `subi -e "var x=10; print(x*2)"`).
- 🦀 **"Rust for SUB-OS" Memory-Safe Kernel Layer (`rust/src/`)**:
  - **Freestanding Bare-Metal Rust**: Compiled with `rustc 1.75+` in pure `no_std` mode with static FFI bindings.
  - **Memory-Safe Cryptography**: RFC-8439 ChaCha20 stream cipher, CSPRNG entropy source, FIPS-202 SHA3-256 (Keccak-f[1600]), FIPS-197 AES-128, and RFC-4648 Base64 codec.
  - **Storage & Disk Parsing**: Memory-safe MBR and GUID Partition Table (GPT) header and partition decoder.
  - **Fast VFS Directory Cache (`dcache`)**: 64-entry LRU path hash table for single-cycle file and directory lookups.
  - **Kernel Health Watchdog & Heartbeat Monitor**: Subsystem sanity checker and anomaly detector.
  - **Zero-Copy JSON Tokenizer**: High-speed JSON key-value query engine for kernel configuration and REST API parsing.
  - **NetFilter Packet Evaluator**: Stateful firewall rule matcher and packet counter.
- 🎯 **Multi-Architecture HAL**:
  - **`x86_64` (AMD64 / Intel 64)**: Custom 2-stage MBR bootloader, E820 BIOS memory mapping, 4-level PML4 48-bit paging, GDT, 256-entry IDT, 8259 PIC, 8254 PIT timer, Hardware FPU/SSE (CR4.OSFXSR).
  - **`aarch64` (ARMv8-A 64-Bit)**: Exception levels (EL1), 16-entry vector table, ARM Generic Interrupt Controller (GICv2), ARM Generic Arch Timer (100 Hz), Stage-1 39-bit VA MMU, PL011 UART.
  - **`armv8i` (ARM32 / AArch32)**: Banked exception mode stacks (IRQ, ABT, UND, SVC), VMSAv7 1MB Section MMU, GICv2 distributor, CP15 Generic Timer, VFP/NEON FPU, PL011 UART.
- 🎛️ **Linux-Style Kconfig TUI Configurator (`make configure`)**:
  - Full interactive `lxdialog` GUI interface (Royal Blue backdrop, floating modal windows, drop shadows, and menu hierarchy).
  - Dynamic Kbuild-style selective compilation based on `.config` and `include/config/autoconf.h`.
- 📁 **Storage & Filesystem Layer**:
  - **FAT32 (VFAT)** storage driver with directory traversal and `/mnt/fat32` auto-mounting.
  - **Second Extended (EXT2)** Linux filesystem driver.
  - **Virtual File System (VFS)**: POSIX `open`, `read`, `write`, `close`, `seek`, `mount` interface.
  - Synthetic **DevFS** (`/dev`), **ProcFS** (`/proc`), and **Sysfs KObject Hierarchy** (`/sys`).
  - Dynamic in-memory RAM disk (`/dev/ram0`).
- 🖧 **Hardware Device Drivers**:
  - **Storage Controllers**: NVMe PCIe SSD, AHCI SATA 6Gb/s, IDE/ATA Hard Disks, VirtIO-Block.
  - **Network Adapters**: Intel 82540EM Gigabit Ethernet, Realtek RTL8139 Fast Ethernet, VirtIO-Net 10GbE.
  - **Multimedia & Display**: Bochs/VBE dynamic resolution video adapter, 2D TrueColor Canvas Rasterizer, Intel High Definition Audio (Azalia HDA), AC'97 sound codec, Formant TTS synthesizer, 8-Bit Melody player.
  - **Bus & Sensors**: PCI bus enumerator, USB 3.0 Extensible Host Controller (xHCI), HWMON CPU temperature and fan tachometer sensors, VirtIO-RNG hardware entropy generator, Unix98 PTY subsystem.
- 🌐 **Networking, Firewalls & Daemons**:
  - Full L2–L4 IPv4 protocol stack with dynamic ARP, ICMP Echo (`ping`), UDP, and full 3-way handshake TCP state machine.
  - **NetFilter** stateful packet inspection firewall (iptables).
  - Built-in **OpenSSH 2.0 Daemon (`sshd`)** on port 2222.
  - Built-in **Micro HTTP REST API Server (`httpd`)** on port 8080.
- 🔒 **Kernel Security & Async Engine**:
  - **Linux Security Module (LSM)** enforcing POSIX capabilities and DAC.
  - **X.509 Cryptographic Keyring** with signature verification.
  - In-Kernel **eBPF Register Virtual Machine** and verifier.
  - **io_uring** lockless asynchronous Submission/Completion ring buffers.
  - Preemptive multi-tasking scheduler with Round-Robin quantum and spinlock synchronization.
  - Systemd-style unit manager (`systemctl`), cron background scheduler (`crond`), and RFC 5424 Syslog engine.
- 🧰 **LazyBox Userland Suite (85+ Linux, Rust, C++ & SUB-Lang Applets)**:
  - Interactive shell with history, quote-aware tokenization, tab autocompletion, ANSI cursor editing, and script runner (`sh`).
  - GNU-compatible **Nano** visual text editor.
  - Full utilities: `ls`, `cat`, `touch`, `mkdir`, `rm`, `cp`, `pwd`, `cd`, `tree`, `find`, `wc`, `head`, `tail`, `stat`, `df`, `mkfs.vfat`, `hexdump`, `neofetch`, `uname`, `free`, `uptime`, `top`, `ps`, `pstree`, `kill`, `dmesg`, `netstat`, `ifconfig`, `ping`, `traceroute`, `curl`, `ssh`, `tts`, `alsamixer`, `sensors`, `rustinfo`, `chacha20`, `sha3sum`, `base64`, `cryptobench`, `fdisk`, `rfilter`, `dcache`, `watchdog`, `jsonquery`, `subinfo`, `subi`, `subpower`, `subbench`, `subquote`, `cppinfo`, `cpptest`, `snake`, and more.

---

## 🏛️ Comprehensive Architecture

```text
+------------------------------------------------------------------------------------+
|                             USERLAND & SYSTEM SUITE                                |
|  Interactive Shell | GNU Nano | Snake Game | Script Runner (sh) | LazyBox (85+ Apps)|
|  Micro HTTP Server | SSH 2.0 Daemon | Service Manager (systemctl) | Syslog Engine  |
+------------------------------------------------------------------------------------+
|                       SUB LANGUAGE IN-KERNEL VM & MODULES                          |
|  subi Interpreter | OS Signature | CPU Power Governor | Recursive Math Benchmark   |
+------------------------------------------------------------------------------------+
|                      FREESTANDING C++17 OOP KERNEL ENGINE                          |
|  Virtual Method Dispatch | RAII | Polymorphism | Telemetry Classes | Templates     |
+------------------------------------------------------------------------------------+
|                       RUST-FOR-SUB-OS MEMORY-SAFE SUBSYSTEM                        |
|  ChaCha20 CSPRNG | SHA3-256 Keccak | AES-128 | Base64 | GPT/MBR | NetFilter Matcher|
|  VFS DCache Table | Kernel Health Watchdog | Zero-Copy JSON | Hardware Sensors     |
+------------------------------------------------------------------------------------+
|                         VIRTUAL FILE SYSTEM LAYER (VFS)                            |
|  vfs_namei | RAMFS (/) | /dev (devfs) | /proc (procfs) | /sys (sysfs) | FAT32 | ext2|
+------------------------------------------------------------------------------------+
|         NAMESPACES, INTER-PROCESS COMMUNICATION & ASYNC I/O ENGINE                 |
|  PID/UTS Isolation | Pipes | Message Queues | Shared Memory | Semaphores | io_uring|
+------------------------------------------------------------------------------------+
|                   CORE KERNEL SERVICES, SECURITY & TRACING                         |
|  Preemptive Scheduler | Fast Syscalls | Dynamic Modules | eBPF Virtual Machine     |
|  LSM POSIX Capabilities | X.509 Keyring | Workqueues | Telemetry & Kernel Tracer   |
+------------------------------------------------------------------------------------+
|               NETWORKING, NETFILTER FIREWALL & SOUND ARCHITECTURE                  |
|  Ethernet | ARP | IPv4 | ICMP | UDP | TCP | DHCP | DNS | Sockets | NetFilter FW   |
|  Embedded HTTP Server (8080) | SSH Server (2222) | Intel HDA | AC'97 | Formant TTS |
+------------------------------------------------------------------------------------+
|                             DEVICE DRIVER TREE                                     |
|  NVMe PCIe SSD | AHCI SATA | ATA IDE | VirtIO-Blk | Ramdisk (/dev/ram0)            |
|  Intel E1000 NIC | RTL8139 NIC | VirtIO-Net | USB 3.0 xHCI | HWMON Sensors         |
|  Bochs VBE Display | 2D Canvas | VGA / TTY1-4 | 16550 Serial | PL011 UART          |
+------------------------------------------------------------------------------------+
|                        HARDWARE ABSTRACTION LAYER (HAL)                            |
|  x86_64 (AMD64/PML4) | aarch64 (ARMv8-A/GICv2/MMU) | armv8i (AArch32/Section MMU)  |
+------------------------------------------------------------------------------------+
```

---

## 🚀 Quick Start Guide

### 1. Prerequisites

Install the standard build essentials, cross-compilers, and Rust toolchain:

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install build-essential g++ nasm qemu-system-x86 qemu-system-arm \
                 gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
                 gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf \
                 rustc cargo dialog python3
```

---

### 2. Linux-Style TUI Configuration

Launch the interactive Kconfig configurator:

```bash
make configure
```
*(Also available via `make menuconfig`)*

#### Fast Architecture Presets:
```bash
make x86_64_defconfig    # Configure for x86_64 (Intel / AMD 64-Bit)
make aarch64_defconfig   # Configure for 64-bit ARM (Cortex-A57)
make armv8i_defconfig    # Configure for 32-bit ARM (Cortex-A15)
```

---

### 3. Compilation & Live Emulation

#### Target: `x86_64` (Default)
```bash
make x86_64_defconfig
make
make run
```
*Port forwardings automatically configured in QEMU:*
- **SSH Server**: `ssh root@localhost -p 2222` (Password: `root`)
- **HTTP REST API**: `http://localhost:8080/api/status`

#### Target: `aarch64` (ARMv8-A 64-Bit)
```bash
make aarch64_defconfig
make ARCH=aarch64
make run ARCH=aarch64
```

#### Target: `armv8i` (ARM32 / AArch32)
```bash
make armv8i_defconfig
make ARCH=armv8i
make run ARCH=armv8i
```

#### Graphical Window Emulation (Optional SDL/GTK GUI)
```bash
make run-gui
```

---

## 🧰 LazyBox Command Matrix (85+ Utilities)

| Category | Commands Included |
|---|---|
| **Core & Shell** | `lazybox`, `sh`, `echo`, `env`, `export`, `whoami`, `id`, `date`, `cal`, `jsonquery`, `subi` |
| **Filesystem & Search** | `nano`, `ls`, `cat`, `touch`, `mkdir`, `rm`, `cp`, `pwd`, `cd`, `tree`, `find`, `wc`, `head`, `tail`, `stat`, `df`, `mkfs.vfat`, `grep`, `hexdump`, `dcache` |
| **Networking & Routing** | `ifconfig`, `ping`, `traceroute`, `arp`, `dhclient`, `nslookup`, `dnscache`, `netstat`, `iptables`, `rfilter`, `httpd`, `sshd`, `curl`, `wget`, `ssh` |
| **Hardware & Storage** | `lspci`, `lsdev`, `lsblk`, `fdisk`, `hdparm`, `sensors`, `speaker`, `beep`, `mouse`, `alsamixer`, `tts`, `virtinfo` |
| **Security & Cryptography** | `su`, `passwd`, `useradd`, `certcheck`, `capsh`, `ipcs`, `chacha20`, `sha3sum`, `base64`, `cryptobench` |
| **SUB Language & System** | `subinfo`, `subpower`, `subbench`, `subquote`, `pstree`, `kill`, `shm`, `systemctl`, `service`, `crontab`, `logger`, `logread`, `watchdog`, `rustinfo`, `cppinfo`, `cpptest` |
| **Entertainment & Games** | `snake`, `matrix`, `calc` |
| **Kernel & Tracing** | `lsmod`, `insmod`, `rmmod`, `slabinfo`, `trace`, `unshare`, `io_uring_test` |
| **Diagnostics & Metrics**| `neofetch`, `uname`, `free`, `uptime`, `top`, `htop`, `ps`, `dmesg`, `vmstat`, `iostat` |
| **System Control** | `clear`, `help`, `sleep`, `reboot`, `shutdown`, `poweroff`, `tty` |

---

## 📦 Official Release

Download precompiled kernel binaries and bootable disk images from GitHub:
👉 **[SUB-OS v0.0.1-beta Release](https://github.com/subhobhai943/SUB-OS/releases/tag/v0.0.1-beta)**

---

## 📄 License

SUB-OS is distributed under the open-source **MIT License**. See [`LICENSE`](LICENSE) for details.
