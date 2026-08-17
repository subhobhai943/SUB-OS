#include <kernel/trace.h>
#include <kernel/task.h>
#include <arch/x86_64/pit.h>
#include <lib/string.h>
#include <kernel/printk.h>

static trace_entry_t trace_buffer[TRACE_BUFFER_SIZE];
static size_t trace_head = 0;
static size_t trace_total = 0;
static bool trace_enabled = true;

void trace_init(void) {
    memset(trace_buffer, 0, sizeof(trace_buffer));
    trace_head = 0;
    trace_total = 0;
    trace_enabled = true;

    trace_record(TRACE_CAT_SYS, "Kernel Tracing Ringbuffer initialized");
    printk(KERN_INFO "TRACE: High-performance Kernel Event & Latency Tracer active\n");
}

void trace_record(trace_category_t cat, const char* msg) {
    if (!trace_enabled || !msg) return;

    trace_entry_t* entry = &trace_buffer[trace_head];
    entry->timestamp = pit_get_ticks();
    entry->cpu_id = 0;
    entry->pid = (uint32_t)task_get_pid();
    entry->category = cat;
    strncpy(entry->message, msg, TRACE_MSG_LEN - 1);
    entry->message[TRACE_MSG_LEN - 1] = '\0';

    trace_head = (trace_head + 1) % TRACE_BUFFER_SIZE;
    if (trace_total < TRACE_BUFFER_SIZE) {
        trace_total++;
    }
}

void trace_enable(bool enable) {
    trace_enabled = enable;
}

bool trace_is_enabled(void) {
    return trace_enabled;
}

void trace_clear(void) {
    memset(trace_buffer, 0, sizeof(trace_buffer));
    trace_head = 0;
    trace_total = 0;
}

size_t trace_get_count(void) {
    return trace_total;
}

const trace_entry_t* trace_get_entry(size_t index) {
    if (index >= trace_total) return NULL;
    size_t actual_idx = (trace_head - trace_total + index + TRACE_BUFFER_SIZE) % TRACE_BUFFER_SIZE;
    return &trace_buffer[actual_idx];
}
