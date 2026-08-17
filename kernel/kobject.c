#include <kernel/kobject.h>
#include <kernel/printk.h>
#include <mm/kmalloc.h>
#include <lib/string.h>

static kobject_t kobject_pool[MAX_KOBJECTS];
static kobject_t* sys_root = NULL;
static kobject_t* sys_devices = NULL;
static kobject_t* sys_bus = NULL;
static kobject_t* sys_class = NULL;
static kobject_t* sys_kernel = NULL;

void kobject_subsystem_init(void) {
    memset(kobject_pool, 0, sizeof(kobject_pool));

    sys_root    = kobject_create_and_add("sys", KOBJ_TYPE_SUBSYS, NULL);
    sys_devices = kobject_create_and_add("devices", KOBJ_TYPE_SUBSYS, sys_root);
    sys_bus     = kobject_create_and_add("bus", KOBJ_TYPE_SUBSYS, sys_root);
    sys_class   = kobject_create_and_add("class", KOBJ_TYPE_SUBSYS, sys_root);
    sys_kernel  = kobject_create_and_add("kernel", KOBJ_TYPE_SUBSYS, sys_root);

    // Populate standard buses
    kobject_t* pci_bus = kobject_create_and_add("pci", KOBJ_TYPE_BUS, sys_bus);
    kobject_t* usb_bus = kobject_create_and_add("usb", KOBJ_TYPE_BUS, sys_bus);
    (void)pci_bus; (void)usb_bus;

    // Populate standard classes
    kobject_create_and_add("net", KOBJ_TYPE_CLASS, sys_class);
    kobject_create_and_add("block", KOBJ_TYPE_CLASS, sys_class);
    kobject_create_and_add("sound", KOBJ_TYPE_CLASS, sys_class);
    kobject_create_and_add("tty", KOBJ_TYPE_CLASS, sys_class);

    // Populate standard devices
    kobject_create_and_add("pci0000:00:03.0_e1000", KOBJ_TYPE_DEVICE, sys_devices);
    kobject_create_and_add("pci0000:00:01.1_ata", KOBJ_TYPE_DEVICE, sys_devices);
    kobject_create_and_add("pci0000:00:02.0_vga", KOBJ_TYPE_DEVICE, sys_devices);

    printk(KERN_INFO "KOBJECT: Unified Device Model & Sysfs Object Tree online (/sys/)\n");
}

kobject_t* kobject_create_and_add(const char* name, kobj_type_t type, kobject_t* parent) {
    if (!name) return NULL;

    for (size_t i = 0; i < MAX_KOBJECTS; i++) {
        if (!kobject_pool[i].in_use) {
            kobject_pool[i].in_use = true;
            strncpy(kobject_pool[i].name, name, KOBJ_NAME_LEN - 1);
            kobject_pool[i].type = type;
            kobject_pool[i].parent = parent;
            kobject_pool[i].ref_count = 1;

            if (parent) {
                snprintf(kobject_pool[i].path, sizeof(kobject_pool[i].path), "%s/%s", parent->path, name);
            } else {
                snprintf(kobject_pool[i].path, sizeof(kobject_pool[i].path), "/%s", name);
            }

            return &kobject_pool[i];
        }
    }
    return NULL;
}

void kobject_get(kobject_t* kobj) {
    if (kobj && kobj->in_use) kobj->ref_count++;
}

void kobject_put(kobject_t* kobj) {
    if (kobj && kobj->in_use) {
        if (kobj->ref_count > 0) kobj->ref_count--;
        if (kobj->ref_count == 0) kobj->in_use = false;
    }
}

size_t kobject_get_total_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < MAX_KOBJECTS; i++) {
        if (kobject_pool[i].in_use) count++;
    }
    return count;
}

const kobject_t* kobject_get_at(size_t index) {
    if (index >= MAX_KOBJECTS || !kobject_pool[index].in_use) return NULL;
    return &kobject_pool[index];
}

void kobject_dump_tree(void) {
    printk(ANSI_BRIGHT_CYAN "=== SUB-OS Sysfs KObject Hierarchy Tree (/sys/) ===\n" ANSI_RESET);
    printk(ANSI_YELLOW "REF   TYPE      SYSFS PATH\n" ANSI_RESET);

    for (size_t i = 0; i < MAX_KOBJECTS; i++) {
        if (kobject_pool[i].in_use) {
            const char* type_str = "GENERIC";
            switch (kobject_pool[i].type) {
                case KOBJ_TYPE_BUS:    type_str = "BUS    "; break;
                case KOBJ_TYPE_DEVICE: type_str = "DEVICE "; break;
                case KOBJ_TYPE_CLASS:  type_str = "CLASS  "; break;
                case KOBJ_TYPE_SUBSYS: type_str = "SUBSYS "; break;
                default: break;
            }
            printk("[%2u]  %s   %s\n",
                   kobject_pool[i].ref_count, type_str, kobject_pool[i].path);
        }
    }
}
