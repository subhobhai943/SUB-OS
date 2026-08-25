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

- **Preemptive Weave scheduling from the timer tick** (2026-08-25):
  The PIT tick now drives involuntary context switches. sched_tick() only
  records that a quantum is spent (weave_need_resched); the switch happens at
  IRQ return in sched_preempt_on_return(), after the PIC EOI, on the
  interrupted thread's own kernel stack -- so the tick path stays re-entrancy
  safe and threads unwind cleanly through the normal iretq. The scheduler's
  lock sections were made interrupt-atomic (arch_irq_save/restore, new in
  arch.h) so the timer can never fire mid-mutation, and a preemption-disable
  count (sched_preempt_disable/enable, used by printk) keeps a switch from
  landing while a plain spinlock is held. Fresh preemptible kernel threads opt
  into interrupts at entry. Verified on x86_64 under QEMU: a boot-time self-test
  arms preemption and two NON-yielding worker threads are forced to share the
  CPU -- 20 involuntary switches, both workers advance and are woven down to the
  CPU-bound lanes -- then boot proceeds cleanly to ring-3 init and the shell
  with no faults. (Debugging note: an early re-entrancy guard deadlocked all
  preemption because sched_schedule() does not return to its caller until that
  thread is rescheduled; it was removed, since the masked switch already
  prevents nesting.)

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

1. **Thread reaping / zombie cleanup.** Exited kernel threads currently leak
   their 16 KB stack + task_t. Add a reaper (per-CPU dead list drained by the
   idle thread) so `task_exit` hands the corpse to it instead of leaking.
2. **Boot-test the ARM context switch.** Wire a `weave_selftest()` call into the
   aarch64/armv8i boot paths and boot under `qemu-system-aarch64`/`-arm` to
   confirm the AArch64/AArch32 `sub_ctx_switch` frames are correct.
3. **fork/exec/wait for the ring-3 userland.** Build on the existing ELF64
   loader + per-process page tables so `sh` can spawn and reap real children,
   not just the single builtin init.
4. **An original capability-based IPC channel** ("sub-ports"): typed,
   capability-addressed message endpoints distinct from the SysV msg/pipe/shm
   set already present in `ipc/` — leaning into the "object-first" identity.
5. **SMP bring-up (x86_64):** AP boot trampoline, per-CPU run lanes, and making
   the Weave lanes per-CPU with work-stealing between them.
6. **GUI (feat/gui-desktop):** compositor damage tracking, a VFS-backed file
   manager, a text editor, and a settings panel over the toolkit widgets.

## Notes / gotchas

- `.S` files are run through cpp: no nested `/* ... */` comments.
- `printk`/vsprintf supports only `c s d i u o x X p %` — no length modifiers
  (`l`, `ll`) and no field width; cast to `int`/`unsigned`/`void*`.
- `list_del()` NULLs both links, so `list_empty()` is not a reliable "is this
  node queued?" test after a delete — check the links directly.
- Preemption is armed after boot (step 14). A switch must NOT run while a plain
  spinlock is held: wrap such sections with sched_preempt_disable/enable (printk
  already does). weave_lock sections use arch_irq_save/restore so the tick
  cannot fire inside them.
- During `subos_modular_core_boot()` step 13, hardware interrupts are still
  masked (enabled at step 14), so cooperative in-boot tests are deterministic.
