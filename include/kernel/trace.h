#ifndef _KERNEL_TRACE_H
#define _KERNEL_TRACE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TRACE_BUFFER_SIZE 256
#define TRACE_MSG_LEN     96

typedef enum {
    TRACE_CAT_SYS = 0,
    TRACE_CAT_IRQ,
    TRACE_CAT_SCHED,
    TRACE_CAT_MM,
    TRACE_CAT_NET,
    TRACE_CAT_FS,
    TRACE_CAT_MAX
} trace_category_t;

typedef struct trace_entry {
    uint64_t timestamp;
    uint32_t cpu_id;
    uint32_t pid;
    trace_category_t category;
    char message[TRACE_MSG_LEN];
} trace_entry_t;

void trace_init(void);
void trace_record(trace_category_t cat, const char* msg);
void trace_enable(bool enable);
bool trace_is_enabled(void);
void trace_clear(void);
size_t trace_get_count(void);
const trace_entry_t* trace_get_entry(size_t index);

#endif // _KERNEL_TRACE_H
