#include "paging.h"
#include "early_alloc.h"
#include "string.h"
#include "serial.h"

static uint32_t* page_directory = NULL;

static inline uint32_t page_align_down(uint32_t addr) {
    return addr & ~(PAGE_SIZE - 1u);
}

static inline uint32_t page_align_up(uint32_t addr) {
    return (addr + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
}

static uint32_t* ensure_page_table(uint32_t dir_index, uint32_t map_flags);

void paging_prepare_range(uint32_t vaddr, uint32_t size, uint32_t flags) {
    if (size == 0) {
        return;
    }

    uint32_t start_v = page_align_down(vaddr);
    uint32_t end_v = page_align_up(vaddr + size);

    for (uint32_t va = start_v; va < end_v; va += PAGE_SIZE) {
        uint32_t dir_index = (va >> 22) & 0x3FFu;
        (void)ensure_page_table(dir_index, flags);
    }
}

static uint32_t* ensure_page_table(uint32_t dir_index, uint32_t map_flags) {
    uint32_t entry = page_directory[dir_index];
    if (entry & PAGE_PRESENT) {
        if (map_flags & PAGE_USER) {
            page_directory[dir_index] |= PAGE_USER;
        }
        return (uint32_t*)(entry & 0xFFFFF000u);
    }

    uint32_t* table = (uint32_t*)early_alloc(PAGE_SIZE, PAGE_SIZE);
    memset(table, 0, PAGE_SIZE);
    uint32_t pde_flags = PAGE_PRESENT | PAGE_RW;
    if (map_flags & PAGE_USER) {
        pde_flags |= PAGE_USER;
    }
    page_directory[dir_index] = ((uint32_t)table & 0xFFFFF000u) | pde_flags;
    return table;
}

void paging_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t dir_index = (vaddr >> 22) & 0x3FFu;
    uint32_t tbl_index = (vaddr >> 12) & 0x3FFu;

    uint32_t* table = ensure_page_table(dir_index, flags);
    table[tbl_index] = (paddr & 0xFFFFF000u) | (flags & 0xFFFu);
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

    // Identity-map the first 16 MiB for the kernel and early boot data.
    paging_map_range(0x00000000u, 0x00000000u, 16u * 1024u * 1024u, PAGE_PRESENT | PAGE_RW);

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
