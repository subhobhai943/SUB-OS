#include <kernel/namespace.h>
#include <lib/string.h>
#include <kernel/printk.h>

static nsproxy_t ns_table[MAX_NAMESPACES];
static uint32_t current_ns_id = 0;

void namespace_init(void) {
    memset(ns_table, 0, sizeof(ns_table));

    // Root / Global Namespace (ID 0)
    ns_table[0].id = 0;
    ns_table[0].in_use = true;
    strcpy(ns_table[0].name, "root_ns");
    strcpy(ns_table[0].uts.hostname, "sub-os");
    strcpy(ns_table[0].uts.domainname, "localdomain");
    ns_table[0].pid_offset = 0;
    ns_table[0].isolated_mounts = false;

    current_ns_id = 0;
    printk(KERN_INFO "NAMESPACE: Lightweight Container & Process Isolation layer active\n");
}

nsproxy_t* namespace_create(const char* name, uint32_t flags) {
    for (uint32_t i = 1; i < MAX_NAMESPACES; i++) {
        if (!ns_table[i].in_use) {
            nsproxy_t* ns = &ns_table[i];
            ns->id = i;
            ns->in_use = true;
            strncpy(ns->name, name ? name : "container", sizeof(ns->name) - 1);
            ns->name[sizeof(ns->name) - 1] = '\0';

            if (flags & CLONE_NEWUTS) {
                strncpy(ns->uts.hostname, ns->name, sizeof(ns->uts.hostname) - 1);
            } else {
                strcpy(ns->uts.hostname, ns_table[0].uts.hostname);
            }
            strcpy(ns->uts.domainname, "container.local");

            ns->pid_offset = (flags & CLONE_NEWPID) ? (i * 1000) : 0;
            ns->isolated_mounts = (flags & CLONE_NEWNS) ? true : false;
            return ns;
        }
    }
    return NULL;
}

nsproxy_t* namespace_get_current(void) {
    return &ns_table[current_ns_id];
}

int namespace_switch(uint32_t id) {
    if (id >= MAX_NAMESPACES || !ns_table[id].in_use) return -1;
    current_ns_id = id;
    return 0;
}

size_t namespace_get_count(void) {
    size_t c = 0;
    for (size_t i = 0; i < MAX_NAMESPACES; i++) {
        if (ns_table[i].in_use) c++;
    }
    return c;
}

const nsproxy_t* namespace_get(size_t index) {
    if (index >= MAX_NAMESPACES || !ns_table[index].in_use) return NULL;
    return &ns_table[index];
}
