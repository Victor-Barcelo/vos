#ifndef PAGING_H
#define PAGING_H

#include "types.h"
#include "multiboot.h"

#define PAGE_SIZE 4096u

#define PAGE_PRESENT 0x001u
#define PAGE_RW      0x002u
#define PAGE_USER    0x004u

void paging_init(const multiboot_info_t* mbi);

void paging_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);
void paging_map_range(uint32_t vaddr, uint32_t paddr, uint32_t size, uint32_t flags);
void paging_prepare_range(uint32_t vaddr, uint32_t size, uint32_t flags);

uint32_t paging_get_cr3(void);

#endif
