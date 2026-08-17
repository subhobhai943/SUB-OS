#include <kernel/syslog.h>
#include <arch/x86_64/pit.h>
#include <lib/string.h>
#include <kernel/printk.h>

static syslog_entry_t syslog_ring[SYSLOG_BUFFER_SIZE];
static size_t syslog_head = 0;
static size_t syslog_total = 0;

void syslog_init(void) {
    memset(syslog_ring, 0, sizeof(syslog_ring));
    syslog_head = 0;
    syslog_total = 0;

    syslog_write(LOG_INFO | LOG_KERN, "kernel", "SUB-OS System Logging Daemon (syslogd) started");
    printk(KERN_INFO "SYSLOG: RFC 5424 Kernel Logging Subsystem active\n");
}

void syslog_write(int priority, const char* ident, const char* message) {
    if (!message) return;

    syslog_entry_t* entry = &syslog_ring[syslog_head];
    entry->timestamp = pit_get_ticks() / 100;
    entry->priority = (uint8_t)(priority & 0x07);

    int fac = (priority >> 3) & 0x1F;
    switch (fac) {
        case 0: strcpy(entry->facility_str, "kern"); break;
        case 1: strcpy(entry->facility_str, "user"); break;
        case 3: strcpy(entry->facility_str, "daemon"); break;
        case 4: strcpy(entry->facility_str, "auth"); break;
        default: strcpy(entry->facility_str, "syslog"); break;
    }

    strncpy(entry->ident, ident ? ident : "system", sizeof(entry->ident) - 1);
    entry->ident[sizeof(entry->ident) - 1] = '\0';

    strncpy(entry->message, message, sizeof(entry->message) - 1);
    entry->message[sizeof(entry->message) - 1] = '\0';

    syslog_head = (syslog_head + 1) % SYSLOG_BUFFER_SIZE;
    if (syslog_total < SYSLOG_BUFFER_SIZE) {
        syslog_total++;
    }
}

size_t syslog_get_count(void) {
    return syslog_total;
}

const syslog_entry_t* syslog_get_entry(size_t index) {
    if (index >= syslog_total) return NULL;
    size_t actual_idx = (syslog_head - syslog_total + index + SYSLOG_BUFFER_SIZE) % SYSLOG_BUFFER_SIZE;
    return &syslog_ring[actual_idx];
}
