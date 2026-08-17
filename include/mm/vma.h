#ifndef _MM_VMA_H
#define _MM_VMA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20

#define MAX_VMAS_PER_TASK 32

typedef struct vm_area {
    uint64_t vm_start;
    uint64_t vm_end;
    uint32_t vm_flags;
    uint32_t vm_prot;
    uint64_t vm_pgoff;
    char     vm_name[32];
    bool     in_use;
} vm_area_t;

typedef struct mm_struct {
    uint32_t  pid;
    uint64_t  start_code;
    uint64_t  end_code;
    uint64_t  start_data;
    uint64_t  end_data;
    uint64_t  start_brk;
    uint64_t  brk;
    uint64_t  start_stack;
    vm_area_t vmas[MAX_VMAS_PER_TASK];
    size_t    vma_count;
} mm_struct_t;

void vma_init(void);
int  vma_create_mm(uint32_t pid, mm_struct_t* mm);
uint64_t vma_mmap(uint32_t pid, uint64_t addr, size_t len, int prot, int flags, const char* name);
int  vma_munmap(uint32_t pid, uint64_t addr, size_t len);
const vm_area_t* vma_find(uint32_t pid, uint64_t addr);
void vma_dump_maps(uint32_t pid);

#endif // _MM_VMA_H
