#ifndef _KERNEL_SYSLOG_H
#define _KERNEL_SYSLOG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

#define LOG_KERN    (0<<3)
#define LOG_USER    (1<<3)
#define LOG_DAEMON  (3<<3)
#define LOG_AUTH    (4<<3)
#define LOG_SYSLOG  (5<<3)

#define SYSLOG_BUFFER_SIZE 128
#define SYSLOG_MSG_MAX     128

typedef struct syslog_entry {
    uint64_t timestamp;
    uint8_t  priority;
    char     facility_str[12];
    char     ident[24];
    char     message[SYSLOG_MSG_MAX];
} syslog_entry_t;

void syslog_init(void);
void syslog_write(int priority, const char* ident, const char* message);
size_t syslog_get_count(void);
const syslog_entry_t* syslog_get_entry(size_t index);

#endif // _KERNEL_SYSLOG_H
