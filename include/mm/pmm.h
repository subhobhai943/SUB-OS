#ifndef _MM_PMM_H
#define _MM_PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PMM_PAGE_SIZE 4096

// E820 Memory Map entry structure
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attr;
} __attribute__((packed)) e820_entry_t;

#define E820_TYPE_USABLE        1
#define E820_TYPE_RESERVED      2
#define E820_TYPE_ACPI_RECLAIM  3
#define E820_TYPE_ACPI_NVS      4
#define E820_TYPE_BAD           5

void pmm_init(void* memory_map, uint64_t memory_map_count);
void* pmm_alloc_page(void);
void* pmm_alloc_pages(size_t count);
void pmm_free_page(void* page);
void pmm_free_pages(void* page, size_t count);

uint64_t pmm_get_total_pages(void);
uint64_t pmm_get_used_pages(void);
uint64_t pmm_get_free_pages(void);
uint64_t pmm_get_total_memory(void);
uint64_t pmm_get_usable_memory(void);

#endif // _MM_PMM_H
