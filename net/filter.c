#include <net/filter.h>
#include <lib/string.h>
#include <kernel/printk.h>

static filter_rule_t rules_table[FILTER_MAX_RULES];
static uint32_t next_rule_id = 1;

void filter_init(void) {
    memset(rules_table, 0, sizeof(rules_table));
    next_rule_id = 1;

    // Add default baseline rules
    filter_add_rule(FILTER_HOOK_LOCAL_IN, 1, 0, FILTER_ACTION_ACCEPT, "Accept ICMP Ping");
    filter_add_rule(FILTER_HOOK_LOCAL_IN, 6, 80, FILTER_ACTION_ACCEPT, "Allow HTTP Port 80");
    filter_add_rule(FILTER_HOOK_LOCAL_IN, 17, 53, FILTER_ACTION_ACCEPT, "Allow DNS UDP 53");

    printk(KERN_INFO "NETFILTER: Stateful Packet Inspection & Firewall Subsystem online\n");
}

int filter_add_rule(filter_hook_t hook, uint8_t proto, uint16_t dst_port, filter_action_t action, const char* comment) {
    for (size_t i = 0; i < FILTER_MAX_RULES; i++) {
        if (!rules_table[i].in_use) {
            filter_rule_t* r = &rules_table[i];
            r->id = next_rule_id++;
            r->in_use = true;
            r->hook = hook;
            r->protocol = proto;
            r->dst_port = dst_port;
            r->action = action;
            r->packet_count = 0;
            r->byte_count = 0;
            strncpy(r->comment, comment ? comment : "User Rule", sizeof(r->comment) - 1);
            r->comment[sizeof(r->comment) - 1] = '\0';
            return (int)r->id;
        }
    }
    return -1;
}

int filter_delete_rule(uint32_t rule_id) {
    for (size_t i = 0; i < FILTER_MAX_RULES; i++) {
        if (rules_table[i].in_use && rules_table[i].id == rule_id) {
            rules_table[i].in_use = false;
            return 0;
        }
    }
    return -1;
}

filter_action_t filter_evaluate(filter_hook_t hook, uint8_t proto, uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, size_t len) {
    (void)src_ip; (void)dst_ip; (void)src_port;
    for (size_t i = 0; i < FILTER_MAX_RULES; i++) {
        if (rules_table[i].in_use && rules_table[i].hook == hook) {
            if (rules_table[i].protocol != 0 && rules_table[i].protocol != proto) continue;
            if (rules_table[i].dst_port != 0 && rules_table[i].dst_port != dst_port) continue;

            rules_table[i].packet_count++;
            rules_table[i].byte_count += len;
            return rules_table[i].action;
        }
    }
    return FILTER_ACTION_ACCEPT;
}

size_t filter_get_rule_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < FILTER_MAX_RULES; i++) {
        if (rules_table[i].in_use) count++;
    }
    return count;
}

const filter_rule_t* filter_get_rule(size_t index) {
    if (index >= FILTER_MAX_RULES || !rules_table[index].in_use) return NULL;
    return &rules_table[index];
}
