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
[![Version](https://img.shields.io/badge/version-v0.2.0--lts%20(Enterprise)-purple.svg?style=flat-square)]()
[![Userland](https://img.shields.io/badge/userland-LazyBox%20v2.0.0%20%2B%20Nano%20%2B%20Sh-orange.svg?style=flat-square)]()
[![Networking](https://img.shields.io/badge/network-Intel%20E1000%20%7C%20HTTPD%20%7C%20NetFilter%20Firewall-green.svg?style=flat-square)]()
[![Services](https://img.shields.io/badge/services-systemctl%20%7C%20syslogd%20%7C%20crond-yellow.svg?style=flat-square)]()
[![License](https://img.shields.io/badge/license-MIT-lightgrey.svg?style=flat-square)](LICENSE)

**SUB-OS** is an open-source, production-grade 64-bit modular monolithic operating system engineered from the ground up for the `x86_64` architecture. Designed for both **cloud/bare-metal servers** and **interactive workstation use**, SUB-OS features a lightweight, high-performance modular kernel without unnecessary bloat.

It comes fully equipped with a bare-metal driver tree, **SLAB Object Allocator**, an advanced virtual file system (VFS) with FAT32, ext2, sysfs, and devfs, **Lightweight Process & Container Namespaces**, **Kernel Event Tracing (FTrace ringbuffers)**, **Stateful Packet Inspection (NetFilter Firewall)**, an **Embedded Micro HTTP Web Server & REST API (`httpd`)**, **Systemd-style Service Unit Manager (`systemctl`)**, **RFC 5424 Syslog Telemetry**, **Cron Background Job Scheduler**, **Multi-User SHA-256 Authentication (`su`, `passwd`, `useradd`)**, **Ramdisk (`/dev/ram0`)**, a full L2-L4 networking protocol stack with BSD Sockets, **8-Bit Retro Melody Synth & Formant Text-to-Speech (TTS)** voice synthesizer, AC'97 sound architecture, full-screen visual `nano` editor, and the **LazyBox** multi-call userland environment with **65+ Linux-compatible utilities** and shell scripting (`sh`).

---

## 🏛️ Comprehensive System Architecture

```text
+------------------------------------------------------------------------------------+
|                             USERLAND & SYSTEM SUITE                                |
|  Interactive Shell | GNU Nano 2.0.0 | Script Runner (sh) | LazyBox (65+ Tools)     |
|  Micro HTTP Server | Service Manager (systemctl) | Syslog Reader | Multi-User Auth |
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
|  System Metrics Telemetry | Cron Scheduler Daemon | RFC 5424 Kernel Syslog Engine  |
+------------------------------------------------------------------------------------+
|               NETWORKING, NETFILTER FIREWALL & SOUND ARCHITECTURE                  |
|  Ethernet L2 | ARP | IPv4 | ICMP | UDP | TCP | DHCP | DNS | Sockets | NetFilter FW |
|  Embedded HTTP Server & REST API | AC'97 Audio | Formant TTS | 8-Bit Melody Engine |
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
| **[9/18]** | **L2-L4 Network & NetFilter** | Intel E1000 NIC, NetFilter stateful firewall, ARP, IPv4 routing, ICMP Ping, UDP, TCP, DHCP, DNS & HTTPD. |
| **[10/18]** | **Sound & Melody Synth** | Audio core driver, AC'97 soundcard, 8-Bit Melody player, and Formant Text-to-Speech synthesizer. |
| **[11/18]** | **Keyring, Auth & Namespaces**| Loads X.509 trusted keyring, SHA-256 Shadow Authentication, POSIX Capabilities LSM, and namespaces. |
| **[12/18]** | **IPC Engine** | Anonymous Pipes, System V & POSIX Message Queues, Shared Memory segments, and Semaphore sets. |
| **[13/18]** | **io_uring Subsystem** | Lockless Submission Queue Entry (SQE) & Completion Queue Entry (CQE) asynchronous I/O engine. |
| **[14/18]** | **Multi-VFS Mount** | Root RAMFS, synthetic `/dev` devfs, `/proc` procfs, `/sys` sysfs, FAT32 driver, ext2 driver & initramfs. |
| **[15/18]** | **Services, Syslog & Telemetry**| Starts RFC 5424 Syslog daemon, Telemetry Metrics, Cron scheduler, and Systemd Service Unit Manager. |
| **[16/18]** | **Scheduler & LazyBox** | Starts round-robin preemptive scheduler and launches LazyBox interactive userland environment. |
| **[17/18]** | **Hardware Interrupts** | Enables processor interrupts (`sti`) and transitions to userland shell. |
| **[18/18]** | **Interactive Shell** | Launches multi-TTY interactive terminal console on TTY1 with dynamic CWD prompt. |

---

## 🧰 LazyBox Multi-Call Userland Suite (65+ Utilities)

| Category | Commands Included |
|---|---|
| **Core & Scripting** | `lazybox`, `sh` (Script runner), `echo`, `env`, `export`, `whoami`, `id`, `date` (CMOS RTC), `cal` |
| **Filesystem & Editor** | `nano`, `ls`, `cat`, `touch`, `mkdir`, `pwd`, `cd`, `wc`, `head`, `tail`, `stat`, `cp`, `grep`, `hexdump`, `basename`, `dirname` |
| **Server & Web** | `httpd` (Micro Web Server), `curl`, `wget`, `systemctl` / `service` (Daemon Manager), `crontab` |
| **Security & Logging** | `logger`, `logread` (Syslog viewer), `su`, `passwd`, `useradd`, `certcheck`, `capsh`, `ipcs` |
| **Sound & Voice** | `tts` (Phonetic Formant Voice Synthesizer), `tune` (8-Bit Melody Player), `alsamixer` |
| **Kernel & Tracing** | `lsmod`, `insmod`, `rmmod`, `slabinfo`, `trace`, `unshare` (Namespace containers) |
| **Crypto & Hashes** | `sha256sum`, `md5sum`, `crc32`, `rand` |
| **Network & Firewall** | `ifconfig`, `ping`, `arp`, `dhclient`, `nslookup`, `iptables` (Stateful Firewall), `netstat` |
| **Storage & Devices** | `hdparm`, `lspci`, `speaker`, `mouse` |
| **Virtualization & Async**| `virtinfo`, `io_uring_test` |
| **Monitoring & Telemetry**| `uname`, `free`, `uptime`, `vmstat`, `iostat`, `htop`, `top`, `dmesg`, `ps`, `sleep`, `reboot`, `poweroff` |
| **Terminal** | `clear`, `neofetch`, `help` |

---

## 🌐 Server Operations Demonstration

### 1. Launching Micro HTTP Server & Testing REST API
```text
sub-os:/> httpd start 80
HTTPD: Web Server listening on 0.0.0.0:80 (HTTP/1.1)

sub-os:/> curl /api/status
HTTP/1.1 200 OK
Server: SUB-OS/0.2.0 (x86_64)
Content-Type: application/json
Content-Length: 159
Connection: close

{"os":"SUB-OS","version":"0.2.0-lts (x86_64)","arch":"x86_64","uptime_sec":1,"mem_total_kb":130944,"mem_free_kb":122752,"requests_served":1,"status":"HEALTHY"}
```

### 2. Managing System Services
```text
sub-os:/> systemctl
  UNIT                       LOAD   ACTIVE SUB     DESCRIPTION
  syslogd.service            loaded active running System Logging Daemon
  networking.service         loaded active running IPv4 Network Interface Manager
  httpd.service              loaded active running Embedded Micro HTTP Web Server
  crond.service              loaded active running Periodic Command Scheduler Daemon
  firewall.service           loaded active running NetFilter Stateful Packet Inspection
```

### 3. Multi-User Authentication & Switching
```text
sub-os:/> whoami
root

sub-os:/> su admin
Switched to user 'admin'

sub-os:/> whoami
admin

sub-os:/> id
uid=1000(admin) gid=1000(admin) groups=1000(admin)

sub-os:/> su root
Switched to user 'root'
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
