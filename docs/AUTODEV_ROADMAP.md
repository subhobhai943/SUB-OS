# SUB-OS Autonomous Development Roadmap

This file is the cross-run memory for the continuous-development effort. Each
run: read it, pick the single highest-value NEXT item achievable in one
session, implement + verify it, then move it to DONE with a one-line result and
refine the backlog.

## Known-good build invocations (this host)

- **Toolchain setup:** `apt-get install -y nasm gcc-aarch64-linux-gnu
  binutils-aarch64-linux-gnu gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf
  qemu-system-x86 qemu-system-arm`; for Rust: `rustup target add x86_64-unknown-none`.
- **x86_64:** `make x86_64_defconfig && make CROSS_COMPILE=x86_64-linux-gnu-
  CXX=x86_64-linux-gnu-g++ RUSTC="rustc --target x86_64-unknown-none"`
- **aarch64:** `make aarch64_defconfig && make`
- **armv8i:** `make armv8i_defconfig && make`
- **Boot (x86_64, headless):** `qemu-system-x86_64 -drive format=raw,file=SUB-OS.img
  -m 256M -netdev user,id=net0 -device e1000,netdev=net0 -serial stdio -display none`
  (slow under TCG; the shell waits on input, so a timeout at the prompt is normal).

## DONE

- **Weave scheduler + real kernel-thread context switching** (2026-08-25):
  Replaced the state-relabel-only round-robin with the SUB-OS "Weave" tiered
  scheduler backed by a genuine callee-saved context switch (`sub_ctx_switch`,
  per-arch assembly for x86_64/aarch64/armv8i) and a first-run trampoline.
  Threads run on their own kernel stacks; a task's lane floats on behaviour
  (voluntary yield → woven up, quantum-burn → woven down). Verified on x86_64
  under QEMU: a boot-time self-test runs 3 cooperative kernel threads to
  completion (20 real context switches, correct 1→2→3 round-robin interleave,
  clean return to the boot thread, then boot proceeds to ring-3 init + shell).
  ARM switch asm builds on both targets but is not yet boot-tested.

## NEXT (prioritized backlog)

1. **Preemptive Weave scheduling from the timer tick.** The plumbing exists
   (`sched_set_preempt`, `sched_tick` charges the quantum). Arm it *after* boot
   completes, make the PIT ISR path re-entrancy-safe (switch on IRQ return, not
   mid-handler), and add a preemption self-test showing a CPU-bound thread being
   woven down while an interactive one stays in lane 0. Needs an IRQ-context
   switch (save the full trap frame, not just callee-saved state).
2. **Thread reaping / zombie cleanup.** Exited kernel threads currently leak
   their 16 KB stack + task_t. Add a reaper (per-CPU dead list drained by the
   idle thread) so `task_exit` hands the corpse to it instead of leaking.
3. **Boot-test the ARM context switch.** Wire a `weave_selftest()` call into the
   aarch64/armv8i boot paths and boot under `qemu-system-aarch64`/`-arm` to
   confirm the AArch64/AArch32 `sub_ctx_switch` frames are correct.
4. **fork/exec/wait for the ring-3 userland.** Build on the existing ELF64
   loader + per-process page tables so `sh` can spawn and reap real children,
   not just the single builtin init.
5. **An original capability-based IPC channel** ("sub-ports"): typed,
   capability-addressed message endpoints distinct from the SysV msg/pipe/shm
   set already present in `ipc/` — leaning into the "object-first" identity.
6. **SMP bring-up (x86_64):** AP boot trampoline, per-CPU run lanes, and making
   the Weave lanes per-CPU with work-stealing between them.
7. **GUI (feat/gui-desktop):** compositor damage tracking, a VFS-backed file
   manager, a text editor, and a settings panel over the toolkit widgets.

## Notes / gotchas

- `.S` files are run through cpp: no nested `/* ... */` comments.
- `printk`/vsprintf supports only `c s d i u o x X p %` — no length modifiers
  (`l`, `ll`) and no field width; cast to `int`/`unsigned`/`void*`.
- `list_del()` NULLs both links, so `list_empty()` is not a reliable "is this
  node queued?" test after a delete — check the links directly.
- During `subos_modular_core_boot()` step 13, hardware interrupts are still
  masked (enabled at step 14), so cooperative in-boot tests are deterministic.
