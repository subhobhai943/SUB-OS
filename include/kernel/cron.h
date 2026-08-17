#ifndef _KERNEL_CRON_H
#define _KERNEL_CRON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_CRON_JOBS 16

typedef struct cron_job {
    uint32_t id;
    char command[128];
    uint32_t interval_sec;
    uint64_t last_run_tick;
    uint32_t run_count;
    bool enabled;
    bool in_use;
} cron_job_t;

void cron_init(void);
int  cron_add_job(const char* command, uint32_t interval_sec);
int  cron_remove_job(uint32_t id);
void cron_tick(void);

size_t cron_get_job_count(void);
const cron_job_t* cron_get_job(size_t index);

#endif // _KERNEL_CRON_H
