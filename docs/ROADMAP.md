<div align="center">

<img src="../assets/logo_SUB_OS.png" alt="SUB-OS logo" width="140" height="140" />

# SUB-OS Roadmap

</div>

Where SUB-OS is, and where it's going. This is a living document — priorities
shift as the kernel grows. For the fine-grained, per-session engineering backlog
see [`AUTODEV_ROADMAP.md`](AUTODEV_ROADMAP.md).

**Current release:** [`v1.0.0` — Titan](https://github.com/subhobhai943/SUB-OS/releases/tag/v1.0.0) (stable)

---

## ✅ Shipped in v1.0.0 (Titan)

- **Weave scheduler** — genuine per-architecture context switching with
  behavioural priority lanes; cooperative **and** preemptive (timer-driven).
- **Concurrency core** — wait queues, completions, futexes, tiny RCU.
- **Memory management** — buddy PMM, slab allocator, per-process page tables,
  page cache.
- **VFS** — ext2, FAT32, ramfs, procfs, sysfs, devfs.
- **Networking** — Ethernet/ARP/IPv4/UDP, full TCP (active + passive open),
  DHCP, DNS, NetFilter; `sshd` and `httpd`.
- **TLS 1.3 client** — X25519, ChaCha20-Poly1305, HKDF key schedule (browses
  HTTPS; see the [Security Policy](SECURITY.md) for the cert-verification caveat).
- **Image & compression codecs** — DEFLATE/zlib, PNG, BMP, baseline JPEG.
- **Graphical desktop (SUB-WT)** — 60 FPS compositor, window manager, Terminal,
  Files, System & Network Monitor, a real web browser with inline images, and
  more.
- **Multi-arch** — `x86_64`, `aarch64`, `armv8i`.
- **Linux-style build** — Kconfig TUI, Kbuild, per-arch defconfigs.

---

## 🔜 Near term (1.1.x)

- **Thread reaping / zombie cleanup.** Exited kernel threads currently leak their
  16 KB stack + `task_t`. Add a reaper (a dead list drained by the idle thread).
- **Boot-test the ARM context switch.** Wire `weave_selftest()` into the
  aarch64/armv8i boot paths and verify the AArch64/AArch32 `sub_ctx_switch`
  frames under `qemu-system-aarch64` / `-arm`.
- **`fork`/`exec`/`wait` for ring-3 userland.** Build on the ELF64 loader and
  per-process page tables so `sh` can spawn and reap real children.
- **TLS certificate verification.** Close the biggest documented security gap by
  validating the server chain against a bundled trust store.

---

## 🌗 Mid term (1.x)

- **Capability-based IPC ("sub-ports").** Typed, capability-addressed message
  endpoints distinct from the SysV msg/pipe/shm set already in `ipc/`.
- **SMP bring-up (x86_64).** AP boot trampoline, per-CPU run lanes, and per-CPU
  Weave lanes with work-stealing.
- **Desktop maturity.** Compositor damage tracking, a richer file manager, a
  text editor, and a settings panel over the toolkit widgets.
- **Storage & FS.** Write-back caching improvements, journaling groundwork, and
  broader ext2 write coverage.
- **Exploit mitigations.** Enforce W^X on all mappings, add kernel stack
  canaries, and begin ASLR groundwork.

---

## 🌒 Long term (exploratory)

- **Self-hosting toolchain path** — grow the userland toward building parts of
  SUB-OS on SUB-OS.
- **A native SUB-Language userland** — first-class apps written in `.sb`.
- **Real hardware bring-up** — beyond QEMU, on at least one aarch64 board.
- **Networking depth** — IPv6, a TLS *server*, and a hardened, fuzzed stack.
- **Audio/graphics acceleration** — virtio-gpu 3D paths and a mixer.

---

## How priorities are chosen

Each development pass picks the single highest-value item achievable and
verifiable in one session, implements it, verifies it under QEMU, and refines
the backlog. Correctness and boot-tested verification always gate a feature
before it's called done. See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the
verification discipline.

> Have an idea or want to pick something up? Open an issue or a discussion —
> contributions against any of the items above are very welcome.
