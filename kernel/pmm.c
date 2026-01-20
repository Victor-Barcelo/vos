#include "pmm.h"
#include "early_alloc.h"
#include "string.h"
#include "serial.h"

#define PAGE_SIZE 4096u

static uint8_t* frame_bitmap = NULL;
static uint32_t frame_bitmap_bytes = 0;
static uint32_t frames_total = 0;
static uint32_t frames_free = 0;
static uint32_t early_reserved_end = 0;

static bool bitmap_test(uint32_t frame) {
    uint32_t byte = frame / 8u;
    uint32_t bit = frame % 8u;
    return (frame_bitmap[byte] & (uint8_t)(1u << bit)) != 0;
}

static void bitmap_set(uint32_t frame) {
    uint32_t byte = frame / 8u;
    uint32_t bit = frame % 8u;
    frame_bitmap[byte] |= (uint8_t)(1u << bit);
}

static void bitmap_clear(uint32_t frame) {
    uint32_t byte = frame / 8u;
    uint32_t bit = frame % 8u;
    frame_bitmap[byte] &= (uint8_t)~(1u << bit);
}

static void mark_frame_free(uint32_t frame) {
    if (frame >= frames_total) {
        return;
    }
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        frames_free++;
    }
}

static void mark_frame_used(uint32_t frame) {
    if (frame >= frames_total) {
        return;
    }
    if (!bitmap_test(frame)) {
        bitmap_set(frame);
        if (frames_free > 0) {
            frames_free--;
        }
    }
}

static void mark_region_free(uint32_t base, uint32_t size) {
    if (size == 0) {
        return;
    }
    uint32_t start = base / PAGE_SIZE;
    uint32_t end = (base + size + PAGE_SIZE - 1u) / PAGE_SIZE;
    for (uint32_t f = start; f < end; f++) {
        mark_frame_free(f);
    }
}

static void mark_region_used(uint32_t base, uint32_t size) {
    if (size == 0) {
        return;
    }
    uint32_t start = base / PAGE_SIZE;
    uint32_t end = (base + size + PAGE_SIZE - 1u) / PAGE_SIZE;
    for (uint32_t f = start; f < end; f++) {
        mark_frame_used(f);
    }
}

static uint32_t clamp_u64_to_u32(uint64_t v) {
    if (v > 0xFFFFFFFFull) {
        return 0xFFFFFFFFu;
    }
    return (uint32_t)v;
}

static uint32_t multiboot_max_paddr(const multiboot_info_t* mbi) {
    uint32_t max_end = 0;

    if (mbi && (mbi->flags & MULTIBOOT_INFO_MMAP) && mbi->mmap_addr && mbi->mmap_length) {
        uint32_t addr = mbi->mmap_addr;
        uint32_t end = addr + mbi->mmap_length;
        while (addr < end) {
            const multiboot_mmap_entry_t* e = (const multiboot_mmap_entry_t*)addr;
            if (e->type == 1) {
                uint64_t region_end = e->addr + e->len;
                uint32_t region_end32 = clamp_u64_to_u32(region_end);
                if (region_end32 > max_end) {
                    max_end = region_end32;
                }
            }
            addr += e->size + 4u;
        }
        return max_end;
    }

    if (mbi && (mbi->flags & MULTIBOOT_INFO_MEM)) {
        uint32_t upper_bytes = mbi->mem_upper * 1024u;
        uint32_t end = 0x100000u + upper_bytes;
        return end;
    }

    return 32u * 1024u * 1024u;
}

void pmm_init(uint32_t multiboot_magic, const multiboot_info_t* mbi, uint32_t kernel_end_paddr) {
    (void)multiboot_magic;

    uint32_t max_paddr = multiboot_max_paddr(mbi);
    frames_total = max_paddr / PAGE_SIZE;
    if ((max_paddr % PAGE_SIZE) != 0) {
        frames_total++;
    }
    if (frames_total == 0) {
        frames_total = 1;
    }

    frame_bitmap_bytes = (frames_total + 7u) / 8u;
    frame_bitmap = (uint8_t*)early_alloc(frame_bitmap_bytes, 16);
    memset(frame_bitmap, 0xFF, frame_bitmap_bytes);
    frames_free = 0;

    if (mbi && (mbi->flags & MULTIBOOT_INFO_MMAP) && mbi->mmap_addr && mbi->mmap_length) {
        uint32_t addr = mbi->mmap_addr;
        uint32_t end = addr + mbi->mmap_length;
        while (addr < end) {
            const multiboot_mmap_entry_t* e = (const multiboot_mmap_entry_t*)addr;
            if (e->type == 1) {
                uint32_t base = clamp_u64_to_u32(e->addr);
                uint32_t len = clamp_u64_to_u32(e->len);
                mark_region_free(base, len);
            }
            addr += e->size + 4u;
        }
    } else if (mbi && (mbi->flags & MULTIBOOT_INFO_MEM)) {
        mark_region_free(0, mbi->mem_lower * 1024u);
        mark_region_free(0x100000u, mbi->mem_upper * 1024u);
    }

    // Reserve low memory (BIOS/real-mode, etc.).
    mark_region_used(0, 0x100000u);

    // Reserve the kernel image (loaded at 1MB).
    if (kernel_end_paddr > 0x100000u) {
        mark_region_used(0x100000u, kernel_end_paddr - 0x100000u);
    }

    if (mbi) {
        // Reserve multiboot info, mmap, and modules array + module payloads.
        mark_region_used((uint32_t)mbi, sizeof(*mbi));

        if ((mbi->flags & MULTIBOOT_INFO_MMAP) && mbi->mmap_addr && mbi->mmap_length) {
            mark_region_used(mbi->mmap_addr, mbi->mmap_length);
        }

        if ((mbi->flags & MULTIBOOT_INFO_MODS) && mbi->mods_addr && mbi->mods_count) {
            mark_region_used(mbi->mods_addr, mbi->mods_count * (uint32_t)sizeof(multiboot_module_t));
            const multiboot_module_t* mods = (const multiboot_module_t*)mbi->mods_addr;
            for (uint32_t i = 0; i < mbi->mods_count; i++) {
                uint32_t start = mods[i].mod_start;
                uint32_t end = mods[i].mod_end;
                if (end > start) {
                    mark_region_used(start, end - start);
                }
            }
        }

        if ((mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER) && mbi->framebuffer_addr_high == 0) {
            uint32_t fb_start = mbi->framebuffer_addr_low;
            uint32_t fb_size = mbi->framebuffer_pitch * mbi->framebuffer_height;
            if (fb_start && fb_size) {
                mark_region_used(fb_start, fb_size);
            }
        }
    }

    // Reserve the early allocator region itself (bitmap + early allocations).
    uint32_t early_base = early_alloc_start();
    uint32_t early_end = early_alloc_current();
    if (early_end > early_base) {
        mark_region_used(early_base, early_end - early_base);
    }
    early_reserved_end = early_end;

    serial_write_string("[PMM] frames total=");
    serial_write_dec((int32_t)frames_total);
    serial_write_string(" free=");
    serial_write_dec((int32_t)frames_free);
    serial_write_char('\n');
}

static void pmm_reserve_new_early_alloc(void) {
    uint32_t cur = early_alloc_current();
    if (early_reserved_end == 0) {
        early_reserved_end = early_alloc_start();
    }
    if (cur > early_reserved_end) {
        mark_region_used(early_reserved_end, cur - early_reserved_end);
        early_reserved_end = cur;
    }
}

uint32_t pmm_alloc_frame(void) {
    // Page tables and other boot-time structures may still come from early_alloc()
    // after pmm_init(). Make sure those frames stay reserved.
    pmm_reserve_new_early_alloc();

    for (uint32_t frame = 0; frame < frames_total; frame++) {
        if (!bitmap_test(frame)) {
            mark_frame_used(frame);
            return frame * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free_frame(uint32_t paddr) {
    uint32_t frame = paddr / PAGE_SIZE;
    mark_frame_free(frame);
}

uint32_t pmm_total_frames(void) {
    return frames_total;
}

uint32_t pmm_free_frames(void) {
    return frames_free;
}
