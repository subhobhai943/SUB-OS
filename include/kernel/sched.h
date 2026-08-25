#ifndef _KERNEL_SCHED_H
#define _KERNEL_SCHED_H

#include "task.h"

/*
 * SUB-OS "Weave" scheduler
 * ------------------------
 * A distinctive tiered-rotation scheduler rather than a plain run queue. Every
 * runnable task threads through one of WEAVE_NLANES priority lanes. The
 * dispatcher always pulls from the lowest-numbered non-empty lane and rotates
 * round-robin inside it, so latency-sensitive work stays near the front.
 *
 * The lane a task sits in is not fixed: a task that gives up the CPU
 * voluntarily (sched_yield) is treated as interactive and woven UP a lane,
 * while a task that burns its whole quantum and is preempted (sched_schedule)
 * is woven DOWN. This lets throughput-bound threads and interactive threads
 * coexist without either starving, and it is driven entirely by observed
 * behaviour rather than a static nice value.
 *
 * The mechanism underneath is a real callee-saved context switch
 * (sub_ctx_switch, per-arch assembly); this is genuine multitasking, not a
 * state relabel.
 */

#define WEAVE_NLANES 4

void sched_init(void);
void sched_yield(void);      /* voluntary: rewards the caller (woven up)     */
void sched_schedule(void);   /* involuntary/preemptive: caller woven down    */
void sched_tick(void);       /* timer hook: charges the running quantum      */
void sched_add_task(task_t* task);
void sched_remove_task(task_t* task);

/* Let the currently-running context join or leave the run rotation. The kernel
 * idle/boot thread uses these to briefly participate while it drives a batch
 * of kernel threads to completion. */
void sched_join(void);
void sched_leave(void);

/* Arm or disarm preemptive switching from the timer tick. Cooperative
 * switching (sched_yield / task_exit) always works; preemption is opt-in so
 * early boot can run to completion deterministically. */
void sched_set_preempt(bool armed);

/* Diagnostics + the boot-time context-switch self-test. */
void sched_dump(void);
void weave_selftest(void);

#endif // _KERNEL_SCHED_H
