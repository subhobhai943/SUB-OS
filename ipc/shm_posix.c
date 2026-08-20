// POSIX Shared Memory Subsystem (/dev/shm) for SUB-OS
// Implements shm_open, ftruncate, and shm_unlink functionality

#include <ipc/shm_posix.h>
#include <mm/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

static posix_shm_object_t shm_table[POSIX_SHM_MAX_OBJECTS];

void posix_shm_init(void) {
    memset(shm_table, 0, sizeof(shm_table));
    // Create standard system shared memory segments
    posix_shm_open("/subos_system_metrics", 0x40 | 0x02, 0666);
    posix_shm_truncate(0, 4096);
    posix_shm_open("/wayland_display_shm", 0x40 | 0x02, 0666);
    posix_shm_truncate(1, 65536);

    printk(KERN_INFO "SHM: POSIX /dev/shm Shared Memory subsystem initialized\n");
}

int posix_shm_open(const char* name, int flags, uint32_t mode) {
    if (!name || name[0] != '/') return -1;

    // Check if exists
    for (int i = 0; i < POSIX_SHM_MAX_OBJECTS; i++) {
        if (shm_table[i].active && strcmp(shm_table[i].name, name) == 0) {
            shm_table[i].ref_count++;
            return i;
        }
    }

    // Create new
    for (int i = 0; i < POSIX_SHM_MAX_OBJECTS; i++) {
        if (!shm_table[i].active) {
            strncpy(shm_table[i].name, name, POSIX_SHM_MAX_NAME - 1);
            shm_table[i].name[POSIX_SHM_MAX_NAME - 1] = '\0';
            shm_table[i].memory = NULL;
            shm_table[i].size = 0;
            shm_table[i].mode = mode ? mode : 0644;
            shm_table[i].ref_count = 1;
            shm_table[i].active = true;
            return i;
        }
    }
    return -1;
}

int posix_shm_truncate(int fd, size_t size) {
    if (fd < 0 || fd >= POSIX_SHM_MAX_OBJECTS || !shm_table[fd].active) return -1;

    if (shm_table[fd].memory) {
        kfree(shm_table[fd].memory);
    }

    shm_table[fd].memory = kzalloc(size);
    if (!shm_table[fd].memory && size > 0) {
        shm_table[fd].size = 0;
        return -1;
    }
    shm_table[fd].size = size;
    return 0;
}

int posix_shm_unlink(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < POSIX_SHM_MAX_OBJECTS; i++) {
        if (shm_table[i].active && strcmp(shm_table[i].name, name) == 0) {
            if (shm_table[i].memory) {
                kfree(shm_table[i].memory);
            }
            memset(&shm_table[i], 0, sizeof(posix_shm_object_t));
            return 0;
        }
    }
    return -1;
}

posix_shm_object_t* posix_shm_get(int fd) {
    if (fd < 0 || fd >= POSIX_SHM_MAX_OBJECTS || !shm_table[fd].active) return NULL;
    return &shm_table[fd];
}

void posix_shm_dump(void) {
    printk(ANSI_BRIGHT_CYAN "=== POSIX /dev/shm Shared Memory Registry ===\n" ANSI_RESET);
    printk(ANSI_BOLD "%-4s  %-30s  %10s  %6s  %8s\n" ANSI_RESET, "FD", "NAME", "SIZE (B)", "PERMS", "NREF");
    printk("-----------------------------------------------------------------\n");

    int active_count = 0;
    for (int i = 0; i < POSIX_SHM_MAX_OBJECTS; i++) {
        if (shm_table[i].active) {
            active_count++;
            printk("%-4d  " ANSI_BRIGHT_YELLOW "%-30s" ANSI_RESET "  %10llu  0%03o   %8u\n",
                   i, shm_table[i].name, (uint64_t)shm_table[i].size,
                   shm_table[i].mode, shm_table[i].ref_count);
        }
    }
    if (active_count == 0) {
        printk("  (No active /dev/shm objects)\n");
    }
    printk("\n");
}
