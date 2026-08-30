<div align="center">

<img src="../assets/logo_SUB_OS.png" alt="SUB-OS logo" width="140" height="140" />

# Contributing to SUB-OS

</div>

Thanks for your interest in **SUB-OS** — a multi-architecture, modular monolithic,
Linux-compatible operating system written from scratch in C, C++17, Assembly,
freestanding Rust (`no_std`), and the native SUB Language. This guide covers how
to build, the conventions we follow, and how to get a change merged.

---

## Table of Contents

- [Ground rules](#ground-rules)
- [Development environment](#development-environment)
- [Building](#building)
- [Running & verifying](#running--verifying)
- [Coding style](#coding-style)
- [Kernel gotchas](#kernel-gotchas)
- [Commit & PR conventions](#commit--pr-conventions)
- [Reporting bugs](#reporting-bugs)

---

## Ground rules

- **Verify before you claim.** Nothing is "done" until it builds *and* boots.
  Every crypto primitive and codec is tested against a reference vector (RFC or
  a PIL-generated fixture) **before** it enters the kernel. Hold new code to the
  same bar.
- **No libc, no POSIX host.** The kernel is freestanding. No floating point in
  kernel space (`-mno-sse`); all math is integer.
- **Small, reviewable changes.** One feature or fix per pull request, with a
  clear description of what you verified and how.
- **Be kind.** Assume good faith, keep discussion technical, and respect
  reviewers' time.

---

## Development environment

The project targets `x86_64`, `aarch64`, and `armv8i` (AArch32). On a Debian/
Ubuntu host, install the toolchains and emulators:

```bash
sudo apt update
sudo apt install build-essential g++ nasm qemu-system-x86 qemu-system-arm \
                 gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
                 gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf \
                 rustc cargo dialog python3
```

For the freestanding Rust layer:

```bash
rustup target add x86_64-unknown-none
```

> The x86_64 kernel is normally built with an `x86_64-elf-` cross toolchain. A
> stock `x86_64-linux-gnu-` GCC also works — pass it via `CROSS_COMPILE=`.

---

## Building

SUB-OS uses a Linux-style Kconfig + Kbuild flow. The version/codename lives at
the top of the `Makefile` (`VERSION`/`PATCHLEVEL`/`SUBLEVEL`/`EXTRAVERSION`/
`NAME`) and in [`include/init/version.h`](../include/init/version.h) — keep the
two in sync when you bump a release.

```bash
# Configure (interactive) or pick an architecture preset:
make configure                       # or: make menuconfig
make x86_64_defconfig                # x86_64 (Intel / AMD)
make aarch64_defconfig               # 64-bit ARM (Cortex-A57)
make armv8i_defconfig                # 32-bit ARM (Cortex-A15)

# Build:
make                                 # builds the configured/default ARCH
make ARCH=aarch64                    # or target one explicitly
```

> **Always `make clean` when switching `ARCH`.** Object files from another
> architecture link into a *"file in wrong format"* error otherwise.

---

## Running & verifying

```bash
make run                 # x86_64 disk image in QEMU
make run-gui             # windowed graphical desktop
make run-fullscreen      # fullscreen desktop
make run-vnc             # headless: VNC on localhost:5900
make ARCH=aarch64 run    # ARM targets
```

Default login is **`SUB` / `SUB`**. Pass `nogui`, `text`, `emergency`, or
`single` on the kernel command line to boot to the TTY instead of the desktop.

**Headless verification** (what CI and reviewers use): boot with
`-serial file:serial.log -monitor unix:mon.sock,server,nowait -display none`,
drive the login over the monitor `sendkey` interface, and capture the screen
with `screendump`. For a code path with no UI, add a temporary CLI applet under
`userland/lazybox/` that exercises it over serial, verify, then revert the
temporary hook.

When you submit a change, state plainly **what you ran and what you saw** — build
output, boot log excerpt, screenshot, or reference-vector comparison.

---

## Coding style

- **C:** freestanding C11. Match the surrounding file — 4-space indent, K&R
  braces, `snake_case` for functions and variables, `SCREAMING_SNAKE` for
  macros/constants. Keep comment density and idiom consistent with the module
  you're editing.
- **Assembly:** per-arch under `arch/<arch>/`. `.S` files run through `cpp`, so
  **no nested `/* ... */` comments**.
- **Rust:** `no_std`, no allocator assumptions beyond what the kernel provides.
- **C++:** freestanding C++17, no exceptions/RTTI, no host STL.
- **Headers:** public interfaces in `include/`, with an include guard.
- **No new external dependencies** in kernel space.

---

## Kernel gotchas

These bite newcomers — internalize them before touching the relevant subsystem:

- **`printk`/`vsprintf` are minimal.** Supported conversions are
  `c s d i u o x X p %` only — **no** length modifiers (`l`, `ll`) and **no**
  field width. Cast to `int` / `unsigned` / `void*` at the call site.
- **`list_del()` NULLs both links**, so `list_empty()` is not a reliable "is
  this node still queued?" test after a delete — inspect the links directly.
- **Preemption is armed after boot.** A context switch must never run while a
  plain spinlock is held: wrap such sections with
  `sched_preempt_disable/enable` (as `printk` does). `weave_lock` sections use
  `arch_irq_save/restore` so the timer tick cannot fire inside them.
- **Dead tasks/stacks are not reclaimed yet.** Prefer one long-lived worker
  thread over spawning a task per request.
- **Network checksums are byte-wise on purpose** — word-at-a-time reads get
  reordered around the caller's `hdr->checksum = 0` store under `-O2`.
- **Ethernet frames are padded to 60 bytes** — trim payloads to the IP
  `total_length`, never the frame length.

---

## Commit & PR conventions

- **Commit messages** follow Conventional-Commits-style prefixes:
  `feat(scope): …`, `fix(scope): …`, `build: …`, `docs: …`, `release(vX.Y.Z): …`.
  Write the subject in the imperative and add a body explaining *what changed
  and what you verified*.
- **Branch** off `main` for your work; open a pull request against `main`.
- **One logical change per PR.** Rebase/squash noisy WIP commits before review.
- **CI must pass** (build + kernel verification) before merge.
- In the PR description, include: what the change does, which architectures you
  built, and how you verified boot behaviour.

---

## Reporting bugs

Open a GitHub issue with:

1. **Architecture** and how you built (`defconfig`, `ARCH`, toolchain).
2. **Exact command** that reproduces it.
3. **What you expected** vs. **what happened** — paste the serial log and, for
   the desktop, attach a `screendump`.
4. The commit SHA (`git rev-parse --short HEAD`).

For **security-sensitive** reports, do **not** open a public issue — follow the
[Security Policy](SECURITY.md) instead.

---

Happy hacking. 🐉
