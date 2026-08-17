#ifndef _INIT_SERVICE_H
#define _INIT_SERVICE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_SERVICES 16

typedef enum {
    SERVICE_STATE_STOPPED = 0,
    SERVICE_STATE_STARTING,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_STOPPING,
    SERVICE_STATE_FAILED
} service_state_t;

typedef int (*service_func_t)(void);

typedef struct service_unit {
    uint32_t id;
    char name[32];
    char description[64];
    service_state_t state;
    bool enabled;
    uint32_t pid;
    uint64_t start_time;
    uint32_t restart_count;
    service_func_t start_fn;
    service_func_t stop_fn;
    bool in_use;
} service_unit_t;

void service_manager_init(void);
int  service_register(const char* name, const char* desc, service_func_t start_fn, service_func_t stop_fn, bool enable_at_boot);
int  service_start(const char* name);
int  service_stop(const char* name);
int  service_restart(const char* name);
service_state_t service_get_state(const char* name);

size_t service_get_count(void);
const service_unit_t* service_get_by_index(size_t index);

#endif // _INIT_SERVICE_H
