#ifndef _DRIVERS_CPUFREQ_H
#define _DRIVERS_CPUFREQ_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CPUFREQ_GOV_PERFORMANCE,
    CPUFREQ_GOV_POWERSAVE,
    CPUFREQ_GOV_ONDEMAND,
    CPUFREQ_GOV_CONSERVATIVE,
    CPUFREQ_GOV_SCHEDUTIL,
} cpufreq_gov_type_t;

typedef struct {
    uint32_t current_khz;
    uint32_t min_khz;
    uint32_t max_khz;
    uint32_t voltage_mv;
    cpufreq_gov_type_t governor;
    uint64_t transitions;
} cpufreq_stats_t;

void cpufreq_init(void);
int cpufreq_set_governor(cpufreq_gov_type_t gov);
int cpufreq_set_governor_by_name(const char* name);
int cpufreq_set_frequency(uint32_t target_khz);
cpufreq_stats_t cpufreq_get_stats(void);
void cpufreq_update_load(uint32_t cpu_load_percent);
void cpufreq_dump_info(void);

#endif // _DRIVERS_CPUFREQ_H
