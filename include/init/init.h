#ifndef _INIT_INIT_H
#define _INIT_INIT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_CMDLINE_LEN 256
#define MAX_BOOT_ARGS   16

typedef enum {
    RUNLEVEL_HALT = 0,
    RUNLEVEL_SINGLE = 1,
    RUNLEVEL_MULTIUSER = 3,
    RUNLEVEL_GRAPHICAL = 5,
    RUNLEVEL_REBOOT = 6
} runlevel_t;

typedef struct {
    char key[32];
    char value[64];
} boot_param_t;

void init_early(const char* boot_args);
void init_parse_cmdline(const char* cmdline);
const char* init_get_param(const char* key);
bool init_has_param(const char* key);
runlevel_t init_get_runlevel(void);
void init_set_runlevel(runlevel_t rl);
void init_system_banner(void);

#endif // _INIT_INIT_H
