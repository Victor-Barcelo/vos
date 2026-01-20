#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "types.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u

#define MULTIBOOT_INFO_MEM       (1u << 0)
#define MULTIBOOT_INFO_MODS      (1u << 3)
#define MULTIBOOT_INFO_MMAP      (1u << 6)
#define MULTIBOOT_INFO_FRAMEBUFFER (1u << 12)

typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    uint32_t framebuffer_addr_low;
    uint32_t framebuffer_addr_high;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t framebuffer_reserved;
    union {
        struct {
            uint32_t framebuffer_palette_addr;
            uint16_t framebuffer_palette_num_colors;
            uint16_t framebuffer_palette_reserved;
        } __attribute__((packed));
        struct {
            uint8_t framebuffer_red_field_position;
            uint8_t framebuffer_red_mask_size;
            uint8_t framebuffer_green_field_position;
            uint8_t framebuffer_green_mask_size;
            uint8_t framebuffer_blue_field_position;
            uint8_t framebuffer_blue_mask_size;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed)) multiboot_info_t;

typedef struct multiboot_module {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} __attribute__((packed)) multiboot_module_t;

typedef struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

#endif
