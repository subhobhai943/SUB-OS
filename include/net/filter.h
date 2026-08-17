#ifndef _NET_FILTER_H
#define _NET_FILTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FILTER_MAX_RULES 32

typedef enum {
    FILTER_ACTION_ACCEPT = 0,
    FILTER_ACTION_DROP,
    FILTER_ACTION_REJECT,
    FILTER_ACTION_LOG
} filter_action_t;

typedef enum {
    FILTER_HOOK_PRE_ROUTING = 0,
    FILTER_HOOK_LOCAL_IN,
    FILTER_HOOK_FORWARD,
    FILTER_HOOK_LOCAL_OUT,
    FILTER_HOOK_POST_ROUTING,
    FILTER_HOOK_MAX
} filter_hook_t;

typedef struct filter_rule {
    uint32_t id;
    filter_hook_t hook;
    uint32_t src_ip;
    uint32_t src_mask;
    uint32_t dst_ip;
    uint32_t dst_mask;
    uint8_t  protocol;
    uint16_t src_port;
    uint16_t dst_port;
    filter_action_t action;
    uint64_t packet_count;
    uint64_t byte_count;
    char comment[32];
    bool in_use;
} filter_rule_t;

void filter_init(void);
int filter_add_rule(filter_hook_t hook, uint8_t proto, uint16_t dst_port, filter_action_t action, const char* comment);
int filter_delete_rule(uint32_t rule_id);
filter_action_t filter_evaluate(filter_hook_t hook, uint8_t proto, uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, size_t len);

size_t filter_get_rule_count(void);
const filter_rule_t* filter_get_rule(size_t index);

#endif // _NET_FILTER_H
