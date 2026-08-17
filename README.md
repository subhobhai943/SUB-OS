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
[![Userland](https://img.shields.io/badge/userland-LazyBox%20v2.0.0--pro%20%2B%20Nano%20%2B%20Sh-orange.svg?style=flat-square)]()
[![Networking](https://img.shields.io/badge/network-Intel%20E1000%20%7C%20NetFilter%20Firewall%20%7C%20BSD%20Sockets-green.svg?style=flat-square)]()
[![License](https://img.shields.io/badge/license-MIT-lightgrey.svg?style=flat-square)](LICENSE)

**SUB-OS** is an open-source, production-grade 64-bit modular monolithic operating system engineered from the ground up for the `x86_64` architecture. Built adhering to the internal design principles of modern UNIX/Linux kernels while maintaining an elegant, easy-to-understand codebase, SUB-OS features a bare-metal driver tree, physical and dynamic memory management with a **SLAB Object Allocator**, an advanced virtual file system (VFS) with FAT32, ext2, sysfs, and devfs, **Lightweight Process & Container Namespaces**, **Kernel Event Tracing (FTrace-like ringbuffers)**, **Stateful Packet Inspection (NetFilter Firewall)**, **Ramdisk (`/dev/ram0`)**, a full L2-L4 networking protocol stack with BSD Sockets, **8-Bit Retro Melody Synth & Formant Text-to-Speech (TTS)** voice synthesizer, AC'97 sound architecture, full-screen visual `nano` editor, and the **LazyBox** multi-call userland environment with **55+ Linux-compatible utilities** and shell scripting (`sh`).

---

## 🏛️ Comprehensive System Architecture

```text
+------------------------------------------------------------------------------------+
|                             USERLAND & SYSTEM SUITE                                |
|  Interactive Shell | GNU Nano 2.0.0 | Script Runner (sh) | LazyBox (55+ Tools)     |
+------------------------------------------------------------------------------------+
|                         VIRTUAL FILE SYSTEM LAYER (VFS)                            |
|  vfs_namei | RAMFS (/) | /dev (devfs) | /proc (procfs) | /sys (sysfs) | FAT32 | ext2|
+------------------------------------------------------------------------------------+
|         NAMESPACES, INTER-PROCESS COMMUNICATION & ASYNC I/O ENGINE                 |
|  PID & UTS Isolation | Anonymous Pipes | Message Queues | SHM | Semaphores | io_uring|
+------------------------------------------------------------------------------------+
|                   CORE KERNEL SERVICES, SECURITY & TRACING                         |
|  Preemptive Scheduler | Task Control | POSIX Signals | Fast Syscalls | Workqueues  |
|  High-Res Timer Wheel | Dynamic Module Loader | Kernel Event Tracer | LSM Security  |
+------------------------------------------------------------------------------------+
|               NETWORKING, NETFILTER FIREWALL & SOUND ARCHITECTURE                  |
|  Ethernet L2 | ARP | IPv4 | ICMP | UDP | TCP | DHCP | DNS | Sockets | NetFilter FW |
|  ALSA Sound Core | AC'97 Audio | Formant TTS Synthesizer | 8-Bit Melody Engine     |
+------------------------------------------------------------------------------------+
|                             MEMORY MANAGEMENT (MM)                                 |
|     Physical Page Allocator (PMM)  |  Dynamic Heap  |  SLAB / SLUB Object Cache    |
+------------------------------------------------------------------------------------+
|                             DEVICE DRIVER TREE & VIRT                              |
|   VGA / TTY1-4 | 16550 UART Serial | PS/2 Keyboard | PS/2 Mouse | ATA Hard Disk    |
|   PCI Enumerator | Intel E1000 NIC | Ramdisk (/dev/ram0) | VESA FB | CMOS RTC      |
+------------------------------------------------------------------------------------+
|                        HARDWARE ABSTRACTION LAYER (HAL)                            |
|   x86_64 Long Mode | GDT / TSS | IDT / ISRs | Dual 8259A PIC | 8254 PIT Timer      |
+------------------------------------------------------------------------------------+
```

---

## ⚡ 18-Step Master Boot Pipeline

| Step | Subsystem | Description |
|:---:|:---|:---|
| **[1/18]** | **64-Bit GDT & TSS** | Configures flat kernel/user code & data segments and Task State Segment with dedicated 64KB kernel stack. |
| **[2/18]** | **IDT & ISR Table** | Populates 256-entry interrupt descriptor table with CPU exception handlers and hardware IRQ routing. |
| **[3/18]** | **Dual 8259A PIC** | Remaps master (IRQ 0-7 -> 0x20-0x27) and slave (IRQ 8-15 -> 0x28-0x2F) interrupt controllers. |
| **[4/18]** | **PIT & Timer Wheel** | Configures 8254 PIT channel 0 for 100 Hz (10ms resolution) driving quantum preemption & timer wheel. |
| **[5/18]** | **Input & ACPI** | PS/2 Keyboard scancode decoder, PS/2 Mouse packet tracker, and ACPI power management subsystem. |
| **[6/18]** | **PMM, Heap & SLAB** | 64-bit bitmap frame allocator, 4 MB kernel heap, and high-performance SLAB object cache layer. |
| **[7/18]** | **Block Layer, Ramdisk & ATA** | Generic Block Layer with request queues, Ramdisk (`/dev/ram0`), 'noop' elevator, and ATA driver. |
| **[8/18]** | **PCI, VirtIO, FB & USB** | PCI bus enumerator, VirtIO core, Linear 32-bit Framebuffer graphics, and USB host controller. |
| **[9/18]** | **L2-L4 Network & NetFilter** | Intel E1000 NIC, NetFilter stateful firewall, ARP, IPv4 routing, ICMP Ping, UDP, TCP, DHCP & DNS. |
| **[10/18]** | **Sound & Melody Synth** | Audio core driver, AC'97 soundcard, 8-Bit Melody player, and Formant Text-to-Speech synthesizer. |
| **[11/18]** | **Keyring, LSM & Namespaces** | Loads X.509 trusted keyring, POSIX Capabilities LSM, and initializes container namespaces. |
| **[12/18]** | **IPC Engine** | Anonymous Pipes, System V & POSIX Message Queues, Shared Memory segments, and Semaphore sets. |
| **[13/18]** | **io_uring Subsystem** | Lockless Submission Queue Entry (SQE) & Completion Queue Entry (CQE) asynchronous I/O engine. |
| **[14/18]** | **Multi-VFS Mount** | Root RAMFS, synthetic `/dev` devfs, `/proc` procfs, `/sys` sysfs, FAT32 driver, ext2 driver & initramfs. |
| **[15/18]** | **Kernel Core & Tracing** | POSIX Signals, Workqueues, 64-bit Fast Syscalls, Dynamic Module Loader, and Event Tracer. |
| **[16/18]** | **Scheduler & LazyBox** | Starts round-robin preemptive scheduler and launches LazyBox interactive userland environment. |
| **[17/18]** | **Hardware Interrupts** | Enables processor interrupts (`sti`) and transitions to userland shell. |
| **[18/18]** | **Interactive Shell** | Launches multi-TTY interactive terminal console on TTY1 with dynamic CWD prompt. |

---

## 🧰 LazyBox Multi-Call Userland Suite (55+ Utilities)

| Category | Commands Included |
|---|---|
| **Core & Scripting** | `lazybox`, `sh` (Script runner), `echo`, `env`, `export`, `whoami`, `id`, `date` (CMOS RTC), `cal` |
| **Filesystem & Editor** | `nano`, `ls`, `cat`, `touch`, `mkdir`, `pwd`, `cd`, `wc`, `head`, `tail`, `stat`, `cp`, `grep`, `hexdump`, `basename`, `dirname` |
| **Sound & Voice** | `tts` (Phonetic Formant Voice Synthesizer), `tune` (8-Bit Melody Player), `alsamixer` |
| **Kernel & Tracing** | `lsmod`, `insmod`, `rmmod`, `slabinfo`, `trace`, `unshare` (Namespace containers) |
| **Crypto & Security** | `sha256sum`, `md5sum`, `crc32`, `rand`, `certcheck`, `capsh`, `ipcs` |
| **Network & Firewall** | `ifconfig`, `ping`, `arp`, `dhclient`, `nslookup`, `iptables` (Stateful Firewall) |
| **Storage & Devices** | `hdparm`, `lspci`, `speaker`, `mouse` |
| **Virtualization & Async**| `virtinfo`, `io_uring_test` |
| **System Monitoring** | `uname`, `free`, `uptime`, `dmesg`, `ps`, `top`, `sleep`, `reboot`, `poweroff` |
| **Terminal** | `clear`, `neofetch`, `help` |

---

## 📜 Shell Scripting Example (`.sub` scripts)

Create a script with `nano /test.sub` and execute it with `sh /test.sub`:

```bash
# SUB-OS Automation Script
echo "Starting System Self-Check..."
uname -a
free
slabinfo
iptables
tune startup
echo "All subsystems operational!"
```

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
