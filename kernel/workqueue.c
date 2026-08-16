#include <kernel/workqueue.h>
#include <kernel/printk.h>

static work_struct_t* work_head = NULL;
static work_struct_t* work_tail = NULL;

void workqueue_init(void) {
    work_head = NULL;
    work_tail = NULL;
    printk(KERN_INFO "WORKQUEUE: Kernel Async Worker Thread Queue online\n");
}

int schedule_work(work_struct_t* work) {
    if (!work || work->pending) return -1;

    work->pending = true;
    work->next = NULL;

    if (!work_tail) {
        work_head = work;
        work_tail = work;
    } else {
        work_tail->next = work;
        work_tail = work;
    }
    return 0;
}

void workqueue_run_pending(void) {
    while (work_head) {
        work_struct_t* w = work_head;
        work_head = w->next;
        if (!work_head) work_tail = NULL;

        w->pending = false;
        if (w->func) {
            w->func(w->data);
        }
    }
}
