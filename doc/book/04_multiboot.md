# Chapter 4: The Multiboot Specification

## What is Multiboot?

Multiboot is a specification that defines how a bootloader loads an operating system kernel. By following this specification, our kernel can be loaded by any Multiboot-compliant bootloader (like GRUB, GRUB2, or others).

The specification was created to solve a common problem: every OS had its own boot protocol, requiring custom bootloaders. Multiboot provides a standard interface.

## The Multiboot Header

Every Multiboot kernel must have a header in its first 8192 bytes. The bootloader scans for this header to identify and load the kernel.

### Header Structure

```
+----------------------------------------+
|  Offset   |  Field      |  Value       |
+-----------+-------------+--------------+
|  0        |  Magic      |  0x1BADB002  |
|  4        |  Flags      |  (see below) |
|  8        |  Checksum   |  -(magic+f)  |
+----------------------------------------+
```

The checksum ensures integrity: `magic + flags + checksum = 0`.

### Implementation in boot.asm

```nasm
; Multiboot header constants
MBALIGN  equ 1 << 0            ; Align modules on page boundaries
MEMINFO  equ 1 << 1            ; Provide memory map
VIDMODE  equ 1 << 2            ; Video mode information
FLAGS    equ MBALIGN | MEMINFO | VIDMODE
MAGIC    equ 0x1BADB002        ; Multiboot magic number
CHECKSUM equ -(MAGIC + FLAGS)  ; Must sum to zero

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    ; Additional fields for video mode (when VIDMODE flag is set)
    dd 0                       ; header_addr (unused with ELF)
    dd 0                       ; load_addr
    dd 0                       ; load_end_addr
    dd 0                       ; bss_end_addr
    dd 0                       ; entry_addr
    dd 0                       ; mode_type (0 = linear graphics)
    dd 1280                    ; width
    dd 800                     ; height
    dd 32                      ; depth (bits per pixel)
```

### Flags Explained

| Bit | Name | Description |
|-----|------|-------------|
| 0 | MBALIGN | Align loaded modules on 4KB page boundaries |
| 1 | MEMINFO | Request memory map from bootloader |
| 2 | VIDMODE | Request specific video mode |
| 16 | AOUT_KLUDGE | Use a.out format addresses (not needed with ELF) |

VOS uses flags 0, 1, and 2:
- Memory alignment for modules (initramfs)
- Memory map for physical memory manager
- Video mode for high-resolution framebuffer

## What GRUB Provides

When GRUB loads our kernel, it:

1. **Switches to 32-bit Protected Mode**
   - Sets up a basic GDT
   - Enables the A20 line

2. **Loads our kernel**
   - Reads the ELF file
   - Loads segments at specified addresses
   - Passes the entry point to the CPU

3. **Provides information in registers**
   - EAX = 0x2BADB002 (Multiboot magic)
   - EBX = Pointer to Multiboot info structure

4. **Disables interrupts**
   - We must set up our own IDT before enabling them

5. **Loads modules**
   - initramfs and other modules at specified addresses

## The Multiboot Info Structure

GRUB passes a pointer to this structure in EBX:

```c
typedef struct {
    uint32_t flags;             // Which fields are valid
    uint32_t mem_lower;         // KB of lower memory (below 1MB)
    uint32_t mem_upper;         // KB of upper memory (above 1MB)
    uint32_t boot_device;       // Boot device info
    uint32_t cmdline;           // Kernel command line
    uint32_t mods_count;        // Number of modules
    uint32_t mods_addr;         // Physical address of module list
    // ... symbol table info ...
    uint32_t mmap_length;       // Memory map length
    uint32_t mmap_addr;         // Memory map address
    // ... drives info ...
    // ... config table ...
    uint32_t boot_loader_name;  // Bootloader name string
    // ... APM table ...
    // ... VBE info ...
    uint32_t framebuffer_addr;  // Framebuffer physical address
    uint32_t framebuffer_pitch; // Bytes per row
    uint32_t framebuffer_width; // Width in pixels
    uint32_t framebuffer_height;// Height in pixels
    uint8_t  framebuffer_bpp;   // Bits per pixel
    uint8_t  framebuffer_type;  // 0=indexed, 1=RGB, 2=text
    // ... color info ...
} multiboot_info_t;
```

### Flags Field

Each bit indicates which fields are valid:

| Bit | Field(s) Valid |
|-----|----------------|
| 0 | mem_lower, mem_upper |
| 1 | boot_device |
| 2 | cmdline |
| 3 | mods_count, mods_addr |
| 6 | mmap_length, mmap_addr |
| 9 | boot_loader_name |
| 12 | framebuffer_* |

Always check the flags before using a field:

```c
if (mboot_info->flags & (1 << 6)) {
    // Memory map is valid
    parse_memory_map(mboot_info);
}

if (mboot_info->flags & (1 << 12)) {
    // Framebuffer info is valid
    setup_framebuffer(mboot_info);
}
```

## Memory Map

The memory map is crucial for our physical memory manager:

```c
typedef struct {
    uint32_t size;      // Size of this entry (not including size field)
    uint64_t base_addr; // Base physical address
    uint64_t length;    // Length in bytes
    uint32_t type;      // 1 = available, other = reserved
} mmap_entry_t;
```

### Memory Types

| Type | Meaning |
|------|---------|
| 1 | Available RAM |
| 2 | Reserved |
| 3 | ACPI reclaimable |
| 4 | ACPI NVS |
| 5 | Bad memory |

VOS treats type 1 as available and everything else as reserved.

### Parsing the Memory Map

```c
void parse_memory_map(multiboot_info_t *mboot) {
    mmap_entry_t *mmap = (mmap_entry_t *)mboot->mmap_addr;
    uint32_t mmap_end = mboot->mmap_addr + mboot->mmap_length;

    while ((uint32_t)mmap < mmap_end) {
        if (mmap->type == 1) {
            // Available memory region
            pmm_mark_region_free(mmap->base_addr, mmap->length);
        }

        // Move to next entry (size + 4 because size doesn't include itself)
        mmap = (mmap_entry_t *)((uint32_t)mmap + mmap->size + 4);
    }
}
```

## Module Loading

VOS uses a Multiboot module for the initramfs:

```c
typedef struct {
    uint32_t mod_start;  // Physical address of module start
    uint32_t mod_end;    // Physical address of module end
    uint32_t cmdline;    // Module command line
    uint32_t reserved;
} multiboot_module_t;
```

### GRUB Configuration

```
menuentry "VOS" {
    multiboot /boot/kernel.bin
    module /boot/initramfs.tar
}
```

### Using Modules in the Kernel

```c
if (mboot_info->flags & (1 << 3)) {
    multiboot_module_t *mods = (multiboot_module_t *)mboot_info->mods_addr;

    for (uint32_t i = 0; i < mboot_info->mods_count; i++) {
        uint32_t mod_start = mods[i].mod_start;
        uint32_t mod_size = mods[i].mod_end - mods[i].mod_start;

        // First module is initramfs
        if (i == 0) {
            vfs_init_from_tar(mod_start, mod_size);
        }
    }
}
```

## Framebuffer Information

When the VIDMODE flag is set, GRUB provides framebuffer details:

```c
if ((mboot_info->flags & (1 << 12)) &&
    mboot_info->framebuffer_type == 1) {  // RGB mode

    uint32_t fb_addr = mboot_info->framebuffer_addr;
    uint32_t width = mboot_info->framebuffer_width;
    uint32_t height = mboot_info->framebuffer_height;
    uint32_t pitch = mboot_info->framebuffer_pitch;
    uint8_t bpp = mboot_info->framebuffer_bpp;

    framebuffer_init(fb_addr, width, height, pitch, bpp);
}
```

## Multiboot2

There's also a Multiboot2 specification with additional features:

- 64-bit support
- UEFI boot support
- More flexible tags

VOS uses Multiboot1 for simplicity, but the concepts are similar.

## Summary

The Multiboot specification provides:

1. **Standardized kernel format** - Any Multiboot bootloader can load our kernel
2. **System information** - Memory map, modules, framebuffer
3. **Consistent state** - Protected mode, known register contents
4. **Module support** - Load additional files (initramfs)

This abstraction lets us focus on kernel development without writing bootloader code.

---

*Previous: [Chapter 3: Understanding the Boot Process](03_boot_process.md)*
*Next: [Chapter 5: Assembly Entry Point](05_assembly_entry.md)*
