#ifndef _KERNEL_VT_ART_H
#define _KERNEL_VT_ART_H

#include <stdint.h>

void vt_show_palette_256(void);
void vt_show_load_bars(uint32_t cpu_percent, uint32_t mem_percent, uint32_t disk_percent);
void vt_show_logo_banner(void);

#endif // _KERNEL_VT_ART_H
