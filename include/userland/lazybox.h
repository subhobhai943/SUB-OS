#ifndef _USERLAND_LAZYBOX_H
#define _USERLAND_LAZYBOX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define LAZYBOX_VERSION "2.0.0-pro"

typedef int (*applet_fn_t)(int argc, char** argv);

typedef struct {
    const char* name;
    applet_fn_t func;
    const char* usage;
    const char* desc;
    const char* category;
} lazybox_applet_t;

void lazybox_init(void);
int  lazybox_main(int argc, char** argv);
bool lazybox_has_applet(const char* name);
int  lazybox_run_applet(const char* name, int argc, char** argv);
const lazybox_applet_t* lazybox_get_applets(int* count_out);

#endif // _USERLAND_LAZYBOX_H
