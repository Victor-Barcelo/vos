#include "paging.h"
#include "early_alloc.h"
#include "string.h"
#include "serial.h"

static uint32_t* page_directory = NULL;
static uint32_t* kernel_directory = NULL;

// VOS user address space layout.
static const uint32_t USER_BASE = 0x02000000u;
static const uint32_t USER_LIMIT = 0xC0000000u;

static inline uint32_t page_align_down(uint32_t addr) {
    return addr & ~(PAGE_SIZE - 1u);
}

static inline uint32_t page_align_up(uint32_t addr) {
    return (addr + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
}

static bool is_kernel_vaddr(uint32_t vaddr) {
    return vaddr >= USER_LIMIT;
}

static uint32_t* ensure_page_table(uint32_t* dir, uint32_t dir_index, uint32_t map_flags);

static void copy_shared_kernel_pdes(uint32_t* dst) {
    if (!dst || !kernel_directory || dst == kernel_directory) {
        return;
    }

    uint32_t low_end = USER_BASE >> 22;     // exclusive
    uint32_t high_start = USER_LIMIT >> 22; // inclusive

    // Low identity-mapped region (< USER_BASE).
    for (uint32_t i = 0; i < low_end; i++) {
        dst[i] = kernel_directory[i];
    }

    // High kernel region (>= USER_LIMIT).
    for (uint32_t i = high_start; i < 1024u; i++) {
        dst[i] = kernel_directory[i];
    }

    // Keep the early allocator region visible to the kernel even when running
    // on a user page directory. Large initramfs/modules can push early_alloc()
    // above USER_BASE, and we still need to access page tables/directories
    // stored there while setting up user mappings.
    uint32_t early_start = early_alloc_start();
    uint32_t early_end = early_alloc_current();
    if (early_end > early_start) {
        uint32_t start_idx = early_start >> 22;
        uint32_t end_idx = (early_end - 1u) >> 22;
        for (uint32_t i = start_idx; i <= end_idx && i < high_start; i++) {
            dst[i] = kernel_directory[i];
        }
    }
}

void paging_prepare_range(uint32_t vaddr, uint32_t size, uint32_t flags) {
    if (size == 0) {
        return;
    }

    uint32_t start_v = page_align_down(vaddr);
    uint32_t end_v = page_align_up(vaddr + size);

    for (uint32_t va = start_v; va < end_v; va += PAGE_SIZE) {
        uint32_t dir_index = (va >> 22) & 0x3FFu;

        if (is_kernel_vaddr(va) && kernel_directory) {
            (void)ensure_page_table(kernel_directory, dir_index, flags);
            if (page_directory && page_directory != kernel_directory) {
                page_directory[dir_index] = kernel_directory[dir_index];
            }
        } else {
            (void)ensure_page_table(page_directory, dir_index, flags);
        }
    }
}

static uint32_t* ensure_page_table(uint32_t* dir, uint32_t dir_index, uint32_t map_flags) {
    if (!dir) {
        return NULL;
    }

    uint32_t entry = dir[dir_index];
    if (entry & PAGE_PRESENT) {
        if (map_flags & PAGE_USER) {
            dir[dir_index] |= PAGE_USER;
        }
        return (uint32_t*)(entry & 0xFFFFF000u);
    }

    uint32_t* table = (uint32_t*)early_alloc(PAGE_SIZE, PAGE_SIZE);
    memset(table, 0, PAGE_SIZE);
    uint32_t pde_flags = PAGE_PRESENT | PAGE_RW;
    if (map_flags & PAGE_USER) {
        pde_flags |= PAGE_USER;
    }
    dir[dir_index] = ((uint32_t)table & 0xFFFFF000u) | pde_flags;
    return table;
}

void paging_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t dir_index = (vaddr >> 22) & 0x3FFu;
    uint32_t tbl_index = (vaddr >> 12) & 0x3FFu;

    uint32_t* dir = page_directory;
    if (is_kernel_vaddr(vaddr) && kernel_directory) {
        dir = kernel_directory;
    }

    uint32_t* table = ensure_page_table(dir, dir_index, flags);
    if (!table) {
        return;
    }
    table[tbl_index] = (paddr & 0xFFFFF000u) | (flags & 0xFFFu);

    // Keep kernel mappings shared across all address spaces.
    if (dir == kernel_directory && page_directory && page_directory != kernel_directory) {
        page_directory[dir_index] = kernel_directory[dir_index];
    }
}

bool paging_unmap_page(uint32_t vaddr, uint32_t* out_paddr) {
    uint32_t va = page_align_down(vaddr);
    uint32_t dir_index = (va >> 22) & 0x3FFu;
    uint32_t tbl_index = (va >> 12) & 0x3FFu;

    uint32_t* dir = page_directory;
    if (is_kernel_vaddr(va) && kernel_directory) {
        dir = kernel_directory;
    }

    uint32_t pde = dir[dir_index];
    if ((pde & PAGE_PRESENT) == 0) {
        return false;
    }

    uint32_t* table = (uint32_t*)(pde & 0xFFFFF000u);
    uint32_t pte = table[tbl_index];
    if ((pte & PAGE_PRESENT) == 0) {
        return false;
    }

    if (out_paddr) {
        *out_paddr = pte & 0xFFFFF000u;
    }

    table[tbl_index] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(va) : "memory");
    return true;
}

void paging_map_range(uint32_t vaddr, uint32_t paddr, uint32_t size, uint32_t flags) {
    uint32_t start_v = page_align_down(vaddr);
    uint32_t start_p = page_align_down(paddr);
    uint32_t end_v = page_align_up(vaddr + size);

    for (uint32_t va = start_v, pa = start_p; va < end_v; va += PAGE_SIZE, pa += PAGE_SIZE) {
        paging_map_page(va, pa, flags);
    }
}

static void enable_paging(uint32_t dir_paddr) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(dir_paddr) : "memory");
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

void paging_init(const multiboot_info_t* mbi) {
    page_directory = (uint32_t*)early_alloc(PAGE_SIZE, PAGE_SIZE);
    memset(page_directory, 0, PAGE_SIZE);
    kernel_directory = page_directory;

    // Identity-map enough low physical memory to cover:
    // - The kernel + early boot data
    // - Multiboot structures/modules
    // - early_alloc() allocations (page tables, etc.)
    //
    // Historically we mapped a fixed 16 MiB, but large initramfs/modules can
    // push early_alloc() above that, causing faults immediately after paging
    // is enabled (before IDT is set up).
    uint32_t mapped_end = 0;
    for (;;) {
        uint32_t target_end = USER_BASE;

        uint32_t early_end = early_alloc_current();
        if (early_end > target_end) {
            target_end = early_end;
        }

        // Round up to a 4 MiB boundary to avoid repeated small extensions.
        target_end = (target_end + 0x3FFFFFu) & ~0x3FFFFFu;

        if (mapped_end >= target_end) {
            break;
        }

        paging_map_range(mapped_end, mapped_end, target_end - mapped_end, PAGE_PRESENT | PAGE_RW);
        mapped_end = target_end;

        // Mapping additional regions may allocate new page tables, which bumps
        // early_alloc_current(). Loop until the mapping covers it.
        if (mapped_end >= early_alloc_current()) {
            break;
        }
    }

    // Map the multiboot info and memory map, if present (usually low memory anyway).
    if (mbi) {
        paging_map_range((uint32_t)mbi, (uint32_t)mbi, sizeof(*mbi), PAGE_PRESENT | PAGE_RW);

        if ((mbi->flags & MULTIBOOT_INFO_MMAP) && mbi->mmap_addr && mbi->mmap_length) {
            paging_map_range(mbi->mmap_addr, mbi->mmap_addr, mbi->mmap_length, PAGE_PRESENT | PAGE_RW);
        }

        if ((mbi->flags & MULTIBOOT_INFO_MODS) && mbi->mods_addr && mbi->mods_count) {
            paging_map_range(mbi->mods_addr, mbi->mods_addr, mbi->mods_count * (uint32_t)sizeof(multiboot_module_t), PAGE_PRESENT | PAGE_RW);
            const multiboot_module_t* mods = (const multiboot_module_t*)mbi->mods_addr;
            for (uint32_t i = 0; i < mbi->mods_count; i++) {
                if (mods[i].mod_end > mods[i].mod_start) {
                    paging_map_range(mods[i].mod_start, mods[i].mod_start, mods[i].mod_end - mods[i].mod_start, PAGE_PRESENT | PAGE_RW);
                }
            }
        }

        // Identity-map the framebuffer (bochs-display uses a high physical address).
        if ((mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER) && mbi->framebuffer_addr_high == 0) {
            uint32_t fb_start = mbi->framebuffer_addr_low;
            uint32_t fb_size = mbi->framebuffer_pitch * mbi->framebuffer_height;
            if (fb_start && fb_size) {
                paging_map_range(fb_start, fb_start, fb_size, PAGE_PRESENT | PAGE_RW);
            }
        }
    }

    serial_write_string("[PAGING] enable cr3=");
    serial_write_hex((uint32_t)page_directory);
    serial_write_char('\n');

    enable_paging((uint32_t)page_directory);
}

uint32_t paging_get_cr3(void) {
    uint32_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

uint32_t* paging_kernel_directory(void) {
    return kernel_directory;
}

uint32_t* paging_create_user_directory(void) {
    if (!kernel_directory) {
        return NULL;
    }

    uint32_t* dir = (uint32_t*)early_alloc(PAGE_SIZE, PAGE_SIZE);
    memset(dir, 0, PAGE_SIZE);

    copy_shared_kernel_pdes(dir);

    return dir;
}

void paging_switch_directory(uint32_t* dir) {
    if (!dir) {
        dir = kernel_directory;
    }
    if (!dir || dir == page_directory) {
        return;
    }

    // Keep kernel mappings synced in every address space: kernel heap, kernel stacks,
    // framebuffer, etc. We copy the PDE entries that cover:
    // - Low identity-mapped region (< USER_BASE)
    // - High kernel region (>= USER_LIMIT)
    copy_shared_kernel_pdes(dir);

    page_directory = dir;
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)dir & 0xFFFFF000u) : "memory");
}

bool paging_user_accessible_range(uint32_t vaddr, uint32_t size, bool write) {
    if (size == 0) {
        return true;
    }

    uint32_t end = vaddr + size;
    if (end < vaddr) {
        return false;
    }

    // VOS user address range (matches kernel/elf.c).
    if (vaddr < USER_BASE) {
        return false;
    }
    if (end > 0xC0000000u) {
        return false;
    }

    uint32_t start_v = page_align_down(vaddr);
    uint32_t end_v = page_align_up(end);

    uint32_t cr3 = paging_get_cr3();
    uint32_t* dir = (uint32_t*)(cr3 & 0xFFFFF000u);

    for (uint32_t va = start_v; va < end_v; va += PAGE_SIZE) {
        uint32_t dir_index = (va >> 22) & 0x3FFu;
        uint32_t tbl_index = (va >> 12) & 0x3FFu;

        uint32_t pde = dir[dir_index];
        if ((pde & PAGE_PRESENT) == 0) {
            return false;
        }
        if ((pde & PAGE_USER) == 0) {
            return false;
        }

        uint32_t* table = (uint32_t*)(pde & 0xFFFFF000u);
        uint32_t pte = table[tbl_index];
        if ((pte & PAGE_PRESENT) == 0) {
            return false;
        }
        if ((pte & PAGE_USER) == 0) {
            return false;
        }
        if (write && ((pte & PAGE_RW) == 0)) {
            return false;
        }
    }

    return true;
}
