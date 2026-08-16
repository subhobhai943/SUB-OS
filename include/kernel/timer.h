#ifndef _KERNEL_TIMER_H
#define _KERNEL_TIMER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef void (*timer_callback_t)(uint64_t data);

typedef struct timer_list {
    uint64_t expires;
    timer_callback_t function;
    uint64_t data;
    bool active;
    struct timer_list* next;
} timer_list_t;

void timer_wheel_init(void);
void timer_add(timer_list_t* timer);
void timer_mod(timer_list_t* timer, uint64_t expires);
void timer_del(timer_list_t* timer);
void timer_tick(void);

#endif // _KERNEL_TIMER_H
