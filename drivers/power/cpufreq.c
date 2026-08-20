// Dynamic CPU Frequency Scaling & ACPI CPPC Power Governor Driver
#include <drivers/cpufreq.h>
#include <drivers/acpi.h>
#include <kernel/printk.h>
#include <lib/string.h>

static cpufreq_stats_t g_stats = {
    .current_khz = 2400000,
    .min_khz = 800000,
    .max_khz = 3600000,
    .voltage_mv = 1050,
    .governor = CPUFREQ_GOV_ONDEMAND,
    .transitions = 0,
};

static const char* gov_names[] = {
    "performance",
    "powersave",
    "ondemand",
    "conservative",
    "schedutil"
};

void cpufreq_init(void) {
    g_stats.current_khz = 2400000;
    g_stats.min_khz = 800000;
    g_stats.max_khz = 3600000;
    g_stats.voltage_mv = 1050;
    g_stats.governor = CPUFREQ_GOV_ONDEMAND;
    g_stats.transitions = 1;

    printk(KERN_INFO "CPUFREQ: Dynamic P-State Scaling online (Freq: %u.%03u GHz - %u.%03u GHz, Gov: %s)\n",
           g_stats.min_khz / 1000000, (g_stats.min_khz % 1000000) / 1000,
           g_stats.max_khz / 1000000, (g_stats.max_khz % 1000000) / 1000,
           gov_names[g_stats.governor]);
}

int cpufreq_set_governor(cpufreq_gov_type_t gov) {
    if (gov > CPUFREQ_GOV_SCHEDUTIL) return -1;
    g_stats.governor = gov;

    switch (gov) {
        case CPUFREQ_GOV_PERFORMANCE:
            cpufreq_set_frequency(g_stats.max_khz);
            break;
        case CPUFREQ_GOV_POWERSAVE:
            cpufreq_set_frequency(g_stats.min_khz);
            break;
        default:
            break;
    }
    return 0;
}

int cpufreq_set_governor_by_name(const char* name) {
    if (!name) return -1;
    for (int i = 0; i <= CPUFREQ_GOV_SCHEDUTIL; i++) {
        if (strcmp(name, gov_names[i]) == 0) {
            return cpufreq_set_governor((cpufreq_gov_type_t)i);
        }
    }
    return -1;
}

int cpufreq_set_frequency(uint32_t target_khz) {
    if (target_khz < g_stats.min_khz) target_khz = g_stats.min_khz;
    if (target_khz > g_stats.max_khz) target_khz = g_stats.max_khz;

    if (target_khz != g_stats.current_khz) {
        g_stats.current_khz = target_khz;
        g_stats.voltage_mv = 850 + ((target_khz - g_stats.min_khz) * 350) / (g_stats.max_khz - g_stats.min_khz);
        g_stats.transitions++;
    }
    return 0;
}

void cpufreq_update_load(uint32_t cpu_load_percent) {
    if (g_stats.governor == CPUFREQ_GOV_ONDEMAND) {
        if (cpu_load_percent > 75) {
            cpufreq_set_frequency(g_stats.max_khz);
        } else if (cpu_load_percent < 25) {
            cpufreq_set_frequency(g_stats.min_khz);
        } else {
            uint32_t target = g_stats.min_khz + ((g_stats.max_khz - g_stats.min_khz) * cpu_load_percent) / 100;
            cpufreq_set_frequency(target);
        }
    }
}

cpufreq_stats_t cpufreq_get_stats(void) {
    return g_stats;
}

void cpufreq_dump_info(void) {
    printk(ANSI_BRIGHT_CYAN "=== CPU Frequency Scaling & Power Management Telemetry ===\n" ANSI_RESET);
    printk("  Current Frequency : \x1b[93m%u.%03u GHz\x1b[0m (%u kHz)\n",
           g_stats.current_khz / 1000000, (g_stats.current_khz % 1000000) / 1000, g_stats.current_khz);
    printk("  Frequency Range   : %u.%03u GHz - %u.%03u GHz\n",
           g_stats.min_khz / 1000000, (g_stats.min_khz % 1000000) / 1000,
           g_stats.max_khz / 1000000, (g_stats.max_khz % 1000000) / 1000);
    printk("  Core Voltage      : \x1b[92m%u mV\x1b[0m (%u.%03u V)\n",
           g_stats.voltage_mv, g_stats.voltage_mv / 1000, g_stats.voltage_mv % 1000);
    printk("  Active Governor   : \x1b[96m%s\x1b[0m\n", gov_names[g_stats.governor]);
    printk("  P-State Changes   : %llu transitions\n", g_stats.transitions);
    printk("  Supported Govs    : performance powersave ondemand conservative schedutil\n\n");
}
