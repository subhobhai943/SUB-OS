#ifndef _KERNEL_METRICS_H
#define _KERNEL_METRICS_H

#include <stdint.h>
#include <stddef.h>

typedef struct system_metrics {
    uint32_t cpu_user_pct;
    uint32_t cpu_system_pct;
    uint32_t cpu_idle_pct;
    uint64_t mem_total_kb;
    uint64_t mem_used_kb;
    uint64_t mem_free_kb;
    uint64_t net_rx_bytes;
    uint64_t net_tx_bytes;
    uint64_t net_rx_packets;
    uint64_t net_tx_packets;
    uint64_t disk_read_sectors;
    uint64_t disk_written_sectors;
} system_metrics_t;

void metrics_init(void);
void metrics_sample(system_metrics_t* out);

#endif // _KERNEL_METRICS_H
