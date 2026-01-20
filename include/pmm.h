#ifndef PMM_H
#define PMM_H

#include "types.h"
#include "multiboot.h"

void pmm_init(uint32_t multiboot_magic, const multiboot_info_t* mbi, uint32_t kernel_end_paddr);

uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t paddr);

uint32_t pmm_total_frames(void);
uint32_t pmm_free_frames(void);

#endif

