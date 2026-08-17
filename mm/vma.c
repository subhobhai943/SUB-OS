#include <mm/vma.h>
#include <mm/kmalloc.h>
#include <kernel/printk.h>
#include <lib/string.h>

#define MAX_TRACKED_TASKS 16

static mm_struct_t task_mm_table[MAX_TRACKED_TASKS];

void vma_init(void) {
    memset(task_mm_table, 0, sizeof(task_mm_table));

    // Initialize kernel/init memory areas
    vma_create_mm(1, &task_mm_table[0]);
    vma_mmap(1, 0x00400000, 0x20000, PROT_READ | PROT_EXEC, MAP_PRIVATE, "/bin/lazybox");
    vma_mmap(1, 0x00600000, 0x10000, PROT_READ | PROT_WRITE, MAP_PRIVATE, "[heap]");
    vma_mmap(1, 0x7FFF0000, 0x08000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, "[stack]");

    printk(KERN_INFO "MM: Virtual Memory Area (VMA) & mmap subsystem initialized\n");
}

int vma_create_mm(uint32_t pid, mm_struct_t* mm) {
    if (!mm) return -1;
    for (size_t i = 0; i < MAX_TRACKED_TASKS; i++) {
        if (task_mm_table[i].pid == 0 || task_mm_table[i].pid == pid) {
            task_mm_table[i].pid = pid;
            task_mm_table[i].start_code = 0x00400000;
            task_mm_table[i].end_code = 0x00420000;
            task_mm_table[i].start_brk = 0x00600000;
            task_mm_table[i].brk = 0x00610000;
            task_mm_table[i].start_stack = 0x7FFF8000;
            task_mm_table[i].vma_count = 0;
            *mm = task_mm_table[i];
            return 0;
        }
    }
    return -1;
}

uint64_t vma_mmap(uint32_t pid, uint64_t addr, size_t len, int prot, int flags, const char* name) {
    if (len == 0) return 0;

    mm_struct_t* mm = NULL;
    for (size_t i = 0; i < MAX_TRACKED_TASKS; i++) {
        if (task_mm_table[i].pid == pid) {
            mm = &task_mm_table[i];
            break;
        }
    }
    if (!mm) {
        vma_create_mm(pid, &task_mm_table[0]);
        mm = &task_mm_table[0];
    }

    uint64_t target_addr = addr;
    if (target_addr == 0) {
        target_addr = 0x70000000 + (mm->vma_count * 0x10000);
    }

    for (size_t i = 0; i < MAX_VMAS_PER_TASK; i++) {
        if (!mm->vmas[i].in_use) {
            mm->vmas[i].vm_start = target_addr;
            mm->vmas[i].vm_end = target_addr + len;
            mm->vmas[i].vm_prot = prot;
            mm->vmas[i].vm_flags = flags;
            mm->vmas[i].vm_pgoff = 0;
            strncpy(mm->vmas[i].vm_name, name ? name : "[anon]", sizeof(mm->vmas[i].vm_name) - 1);
            mm->vmas[i].in_use = true;
            mm->vma_count++;
            return target_addr;
        }
    }
    return 0;
}

int vma_munmap(uint32_t pid, uint64_t addr, size_t len) {
    for (size_t i = 0; i < MAX_TRACKED_TASKS; i++) {
        if (task_mm_table[i].pid == pid) {
            for (size_t j = 0; j < MAX_VMAS_PER_TASK; j++) {
                if (task_mm_table[i].vmas[j].in_use &&
                    task_mm_table[i].vmas[j].vm_start == addr) {
                    task_mm_table[i].vmas[j].in_use = false;
                    task_mm_table[i].vma_count--;
                    return 0;
                }
            }
        }
    }
    return -1;
}

const vm_area_t* vma_find(uint32_t pid, uint64_t addr) {
    for (size_t i = 0; i < MAX_TRACKED_TASKS; i++) {
        if (task_mm_table[i].pid == pid) {
            for (size_t j = 0; j < MAX_VMAS_PER_TASK; j++) {
                if (task_mm_table[i].vmas[j].in_use &&
                    addr >= task_mm_table[i].vmas[j].vm_start &&
                    addr < task_mm_table[i].vmas[j].vm_end) {
                    return &task_mm_table[i].vmas[j];
                }
            }
        }
    }
    return NULL;
}

void vma_dump_maps(uint32_t pid) {
    mm_struct_t* mm = NULL;
    for (size_t i = 0; i < MAX_TRACKED_TASKS; i++) {
        if (task_mm_table[i].pid == pid) {
            mm = &task_mm_table[i];
            break;
        }
    }
    if (!mm) {
        printk(KERN_ERR "pmap: process %u not found\n", pid);
        return;
    }

    printk(ANSI_BRIGHT_CYAN "%u: /bin/lazybox (Process Virtual Memory Maps)\n" ANSI_RESET, pid);
    printk(ANSI_YELLOW "Address           Kbytes   RSS Mode   Mapping\n" ANSI_RESET);

    uint64_t total_kb = 0;
    for (size_t i = 0; i < MAX_VMAS_PER_TASK; i++) {
        if (mm->vmas[i].in_use) {
            uint64_t size_kb = (mm->vmas[i].vm_end - mm->vmas[i].vm_start) / 1024;
            total_kb += size_kb;

            char perm[5] = "---p";
            if (mm->vmas[i].vm_prot & PROT_READ)  perm[0] = 'r';
            if (mm->vmas[i].vm_prot & PROT_WRITE) perm[1] = 'w';
            if (mm->vmas[i].vm_prot & PROT_EXEC)  perm[2] = 'x';
            if (mm->vmas[i].vm_flags & MAP_SHARED)perm[3] = 's';

            printk("%016llx  %6lluKB %5llu %s  %s\n",
                   mm->vmas[i].vm_start, size_kb, size_kb, perm, mm->vmas[i].vm_name);
        }
    }
    printk(ANSI_BRIGHT_CYAN "total %llu KB\n" ANSI_RESET, total_kb);
}
