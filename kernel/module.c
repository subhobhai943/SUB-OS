#include <kernel/module.h>
#include <lib/string.h>
#include <kernel/printk.h>

static kernel_module_t modules_table[MAX_MODULES];
static size_t module_count = 0;

void module_init_subsystem(void) {
    memset(modules_table, 0, sizeof(modules_table));
    module_count = 0;

    // Register built-in kernel modules
    module_load("e1000", NULL, NULL, 48128);
    module_load("ata_piix", NULL, NULL, 32768);
    module_load("ac97_audio", NULL, NULL, 24576);
    module_load("fat32", NULL, NULL, 36864);
    module_load("ext2", NULL, NULL, 40960);
    module_load("tts_synth", NULL, NULL, 16384);

    printk(KERN_INFO "MODULE: Dynamic Kernel Module Loader ready (%llu core modules loaded)\n",
           (uint64_t)module_count);
}

int module_load(const char* name, int (*init_fn)(void), void (*cleanup_fn)(void), size_t size) {
    if (!name || module_count >= MAX_MODULES) return -1;

    kernel_module_t* mod = &modules_table[module_count];
    strncpy(mod->name, name, sizeof(mod->name) - 1);
    strcpy(mod->version, "1.0.0");
    strcpy(mod->author, "SUB-OS Core Team");
    strcpy(mod->description, "Built-in Kernel Subsystem Module");
    mod->size = size > 0 ? size : 4096;
    mod->init = init_fn;
    mod->cleanup = cleanup_fn;
    mod->loaded = true;

    if (init_fn) init_fn();
    module_count++;
    return 0;
}

int module_unload(const char* name) {
    if (!name) return -1;
    for (size_t i = 0; i < module_count; i++) {
        if (strcmp(modules_table[i].name, name) == 0) {
            if (modules_table[i].cleanup) {
                modules_table[i].cleanup();
            }
            modules_table[i].loaded = false;
            return 0;
        }
    }
    return -1;
}

size_t module_get_count(void) {
    return module_count;
}

const kernel_module_t* module_get(size_t index) {
    if (index >= module_count) return NULL;
    return &modules_table[index];
}
