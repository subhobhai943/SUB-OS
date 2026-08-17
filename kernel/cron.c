#include <kernel/cron.h>
#include <arch/x86_64/pit.h>
#include <lib/string.h>
#include <kernel/printk.h>

static cron_job_t cron_table[MAX_CRON_JOBS];
static uint32_t next_job_id = 1;

void cron_init(void) {
    memset(cron_table, 0, sizeof(cron_table));
    next_job_id = 1;

    // Add standard system background jobs
    cron_add_job("syslog_flush", 60);
    cron_add_job("sync", 300);

    printk(KERN_INFO "CROND: Cron Periodic Background Job Scheduler active\n");
}

int cron_add_job(const char* command, uint32_t interval_sec) {
    if (!command || interval_sec == 0) return -1;

    for (size_t i = 0; i < MAX_CRON_JOBS; i++) {
        if (!cron_table[i].in_use) {
            cron_job_t* j = &cron_table[i];
            j->id = next_job_id++;
            j->in_use = true;
            j->enabled = true;
            j->interval_sec = interval_sec;
            j->last_run_tick = pit_get_ticks();
            j->run_count = 0;
            strncpy(j->command, command, sizeof(j->command) - 1);
            j->command[sizeof(j->command) - 1] = '\0';
            return (int)j->id;
        }
    }
    return -1;
}

int cron_remove_job(uint32_t id) {
    for (size_t i = 0; i < MAX_CRON_JOBS; i++) {
        if (cron_table[i].in_use && cron_table[i].id == id) {
            cron_table[i].in_use = false;
            return 0;
        }
    }
    return -1;
}

void cron_tick(void) {
    uint64_t current_ticks = pit_get_ticks();
    for (size_t i = 0; i < MAX_CRON_JOBS; i++) {
        if (cron_table[i].in_use && cron_table[i].enabled) {
            uint64_t delta_ticks = current_ticks - cron_table[i].last_run_tick;
            if (delta_ticks >= (uint64_t)cron_table[i].interval_sec * 100) {
                cron_table[i].last_run_tick = current_ticks;
                cron_table[i].run_count++;
            }
        }
    }
}

size_t cron_get_job_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < MAX_CRON_JOBS; i++) {
        if (cron_table[i].in_use) count++;
    }
    return count;
}

const cron_job_t* cron_get_job(size_t index) {
    if (index >= MAX_CRON_JOBS || !cron_table[index].in_use) return NULL;
    return &cron_table[index];
}
