#include <init/init.h>
#include <init/version.h>
#include <lib/string.h>
#include <kernel/printk.h>

static char raw_cmdline[MAX_CMDLINE_LEN] = "root=/dev/sda console=tty1 init=/bin/lazybox quiet";
static boot_param_t boot_params[MAX_BOOT_ARGS];
static size_t boot_param_count = 0;
static runlevel_t current_runlevel = RUNLEVEL_MULTIUSER;

void init_parse_cmdline(const char* cmdline) {
    if (cmdline && *cmdline) {
        strncpy(raw_cmdline, cmdline, sizeof(raw_cmdline) - 1);
    }

    boot_param_count = 0;
    char buf[MAX_CMDLINE_LEN];
    strncpy(buf, raw_cmdline, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* token = buf;
    while (*token && boot_param_count < MAX_BOOT_ARGS) {
        while (*token == ' ') token++;
        if (!*token) break;

        char* next_space = strchr(token, ' ');
        if (next_space) *next_space = '\0';

        char* eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            strncpy(boot_params[boot_param_count].key, token, sizeof(boot_params[0].key) - 1);
            strncpy(boot_params[boot_param_count].value, eq + 1, sizeof(boot_params[0].value) - 1);
        } else {
            strncpy(boot_params[boot_param_count].key, token, sizeof(boot_params[0].key) - 1);
            boot_params[boot_param_count].value[0] = '\0';
        }
        boot_param_count++;

        if (next_space) token = next_space + 1;
        else break;
    }
}

const char* init_get_param(const char* key) {
    if (!key) return NULL;
    for (size_t i = 0; i < boot_param_count; i++) {
        if (strcmp(boot_params[i].key, key) == 0) {
            return boot_params[i].value;
        }
    }
    return NULL;
}

bool init_has_param(const char* key) {
    if (!key) return false;
    for (size_t i = 0; i < boot_param_count; i++) {
        if (strcmp(boot_params[i].key, key) == 0) {
            return true;
        }
    }
    return false;
}

runlevel_t init_get_runlevel(void) {
    return current_runlevel;
}

void init_set_runlevel(runlevel_t rl) {
    current_runlevel = rl;
}

const char* kernel_get_version(void) {
    return SUBOS_VERSION_STRING;
}

const char* kernel_get_build_banner(void) {
    return "SUB-OS Kernel " SUBOS_VERSION_STRING " (" SUBOS_VERSION_CODENAME ") [" SUBOS_COMPILER "]";
}

void init_early(const char* boot_args) {
    init_parse_cmdline(boot_args);
    printk(KERN_INFO "INIT: Boot arguments parsed: [%s] (%llu params)\n",
           raw_cmdline, (uint64_t)boot_param_count);
}
