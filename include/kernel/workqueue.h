#ifndef _KERNEL_WORKQUEUE_H
#define _KERNEL_WORKQUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef void (*work_func_t)(void* data);

typedef struct work_struct {
    work_func_t func;
    void* data;
    bool pending;
    struct work_struct* next;
} work_struct_t;

void workqueue_init(void);
int schedule_work(work_struct_t* work);
void workqueue_run_pending(void);

#endif // _KERNEL_WORKQUEUE_H
