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
bool paging_unmap_page(uint32_t vaddr, uint32_t* out_paddr);

uint32_t paging_get_cr3(void);

// Returns the kernel address space (shared mappings).
uint32_t* paging_kernel_directory(void);

// Creates a new user address space (shares kernel mappings, empty user region).
uint32_t* paging_create_user_directory(void);

// Switches the current address space by loading CR3.
void paging_switch_directory(uint32_t* dir);

// Validate that a virtual address range is mapped as user-accessible in the
// current address space. If `write` is true, also requires PAGE_RW.
bool paging_user_accessible_range(uint32_t vaddr, uint32_t size, bool write);

#endif
