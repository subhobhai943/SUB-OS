#include <init/service.h>
#include <arch/x86_64/pit.h>
#include <lib/string.h>
#include <kernel/printk.h>

static service_unit_t services_table[MAX_SERVICES];
static size_t registered_count = 0;

static int default_dummy_start(void) { return 0; }
static int default_dummy_stop(void) { return 0; }

void service_manager_init(void) {
    memset(services_table, 0, sizeof(services_table));
    registered_count = 0;

    // Register essential server services
    service_register("syslogd.service", "System Logging Daemon", default_dummy_start, default_dummy_stop, true);
    service_register("networking.service", "IPv4 Network Interface Manager", default_dummy_start, default_dummy_stop, true);
    service_register("httpd.service", "Embedded Micro HTTP Web Server", default_dummy_start, default_dummy_stop, true);
    service_register("crond.service", "Periodic Command Scheduler Daemon", default_dummy_start, default_dummy_stop, true);
    service_register("firewall.service", "NetFilter Stateful Packet Inspection", default_dummy_start, default_dummy_stop, true);

    // Boot all enabled services
    for (size_t i = 0; i < MAX_SERVICES; i++) {
        if (services_table[i].in_use && services_table[i].enabled) {
            service_start(services_table[i].name);
        }
    }

    printk(KERN_INFO "SYSTEMD: Service Unit Manager initialized (5 services online)\n");
}

int service_register(const char* name, const char* desc, service_func_t start_fn, service_func_t stop_fn, bool enable_at_boot) {
    if (!name) return -1;

    for (size_t i = 0; i < MAX_SERVICES; i++) {
        if (!services_table[i].in_use) {
            service_unit_t* s = &services_table[i];
            s->id = (uint32_t)(i + 1);
            s->in_use = true;
            strncpy(s->name, name, sizeof(s->name) - 1);
            s->name[sizeof(s->name) - 1] = '\0';
            strncpy(s->description, desc ? desc : name, sizeof(s->description) - 1);
            s->description[sizeof(s->description) - 1] = '\0';

            s->state = SERVICE_STATE_STOPPED;
            s->enabled = enable_at_boot;
            s->pid = (uint32_t)(100 + i);
            s->start_time = 0;
            s->restart_count = 0;
            s->start_fn = start_fn ? start_fn : default_dummy_start;
            s->stop_fn = stop_fn ? stop_fn : default_dummy_stop;

            if (i >= registered_count) registered_count = i + 1;
            return 0;
        }
    }
    return -1;
}

int service_start(const char* name) {
    if (!name) return -1;
    for (size_t i = 0; i < MAX_SERVICES; i++) {
        if (services_table[i].in_use && strcmp(services_table[i].name, name) == 0) {
            service_unit_t* s = &services_table[i];
            s->state = SERVICE_STATE_STARTING;
            if (s->start_fn) s->start_fn();
            s->state = SERVICE_STATE_RUNNING;
            s->start_time = pit_get_ticks();
            return 0;
        }
    }
    return -1;
}

int service_stop(const char* name) {
    if (!name) return -1;
    for (size_t i = 0; i < MAX_SERVICES; i++) {
        if (services_table[i].in_use && strcmp(services_table[i].name, name) == 0) {
            service_unit_t* s = &services_table[i];
            s->state = SERVICE_STATE_STOPPING;
            if (s->stop_fn) s->stop_fn();
            s->state = SERVICE_STATE_STOPPED;
            return 0;
        }
    }
    return -1;
}

int service_restart(const char* name) {
    service_stop(name);
    return service_start(name);
}

service_state_t service_get_state(const char* name) {
    if (!name) return SERVICE_STATE_FAILED;
    for (size_t i = 0; i < MAX_SERVICES; i++) {
        if (services_table[i].in_use && strcmp(services_table[i].name, name) == 0) {
            return services_table[i].state;
        }
    }
    return SERVICE_STATE_FAILED;
}

size_t service_get_count(void) {
    return registered_count;
}

const service_unit_t* service_get_by_index(size_t index) {
    if (index >= MAX_SERVICES || !services_table[index].in_use) return NULL;
    return &services_table[index];
}
