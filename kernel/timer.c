#include <kernel/timer.h>
#include <arch/x86_64/pit.h>
#include <kernel/printk.h>

static timer_list_t* timer_head = NULL;

void timer_wheel_init(void) {
    timer_head = NULL;
    printk(KERN_INFO "TIMER: Kernel High-Resolution Software Timer Wheel initialized\n");
}

void timer_add(timer_list_t* timer) {
    if (!timer) return;
    timer->active = true;
    timer->next = timer_head;
    timer_head = timer;
}

void timer_mod(timer_list_t* timer, uint64_t expires) {
    if (!timer) return;
    timer->expires = expires;
    if (!timer->active) timer_add(timer);
}

void timer_del(timer_list_t* timer) {
    if (!timer || !timer_head) return;
    timer->active = false;

    if (timer_head == timer) {
        timer_head = timer->next;
        return;
    }

    timer_list_t* curr = timer_head;
    while (curr->next) {
        if (curr->next == timer) {
            curr->next = timer->next;
            return;
        }
        curr = curr->next;
    }
}

void timer_tick(void) {
    uint64_t now = pit_get_ticks();
    timer_list_t* curr = timer_head;
    while (curr) {
        if (curr->active && curr->expires <= now) {
            curr->active = false;
            if (curr->function) curr->function(curr->data);
        }
        curr = curr->next;
    }
}
