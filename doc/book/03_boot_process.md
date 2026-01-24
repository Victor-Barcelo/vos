# Chapter 3: Understanding the Boot Process

## The Boot Sequence

When you power on a computer, the following sequence occurs:

```
+---------------------------------------------------------------------+
|  1. POWER ON                                                        |
|     +-> CPU starts in Real Mode (16-bit)                            |
|         +-> Executes code at address 0xFFFF0 (BIOS ROM)             |
|                                                                     |
|  2. BIOS POST (Power-On Self Test)                                  |
|     +-> Tests RAM, detects hardware                                 |
|         +-> Initializes basic hardware                              |
|                                                                     |
|  3. BIOS BOOT                                                       |
|     +-> Reads first sector (512 bytes) from boot device             |
|         +-> Loads it to address 0x7C00                              |
|             +-> Jumps to 0x7C00 to execute bootloader               |
|                                                                     |
|  4. BOOTLOADER (GRUB in our case)                                   |
|     +-> Switches CPU to Protected Mode (32-bit)                     |
|         +-> Loads kernel into memory                                |
|             +-> Jumps to kernel entry point                         |
|                                                                     |
|  5. KERNEL                                                          |
|     +-> Initializes hardware (IDT, PIC, drivers)                    |
|         +-> Starts user programs                                    |
+-----------------------------------------------------------------   -+
```

## Real Mode vs Protected Mode

### Real Mode (16-bit)

The CPU starts in this legacy mode:

- Can only access 1 MB of memory
- No memory protection between programs
- Segmented memory model: `Physical = Segment * 16 + Offset`
- 16-bit registers and addresses
- Direct access to BIOS services

### Protected Mode (32-bit)

Modern operating systems run in this mode:

- Can access up to 4 GB of memory
- Memory protection via segmentation and paging
- Privilege levels (Ring 0-3) for security
- Flat memory model possible
- 32-bit registers and addresses

GRUB handles the transition to Protected Mode for us, which significantly simplifies our kernel.

## BIOS vs UEFI

### Legacy BIOS

VOS uses legacy BIOS boot:

- Simple 512-byte boot sector
- Real Mode startup
- INT 13h for disk access
- INT 10h for video
- Well-documented and widely supported

### UEFI (Unified Extensible Firmware Interface)

Modern systems use UEFI:

- Starts in 32-bit or 64-bit mode
- More complex boot process
- GPT partition tables
- Secure Boot capability

VOS targets BIOS boot for simplicity, but runs fine on UEFI systems with CSM (Compatibility Support Module) enabled.

## Memory Map at Boot

When the BIOS loads the bootloader:

```
+-------------------------------------+ 0xFFFFFFFF (4 GB)
|                                     |
|         (Unmapped/Reserved)         |
|                                     |
+-------------------------------------+
|                                     |
|         Available Memory            |
|                                     |
+-------------------------------------+ ~0x00100000 (1 MB)
|  BIOS ROM, Video RAM, etc.          |
+-------------------------------------+ 0x000C0000
|  Video Memory (VGA at 0xB8000)      |
+-------------------------------------+ 0x000A0000
|  Extended BIOS Data Area            |
+-------------------------------------+ 0x00080000
|                                     |
|  Conventional Memory                |
|                                     |
+-------------------------------------+ 0x00007E00
|  Bootloader (512 bytes)             |
+-------------------------------------+ 0x00007C00
|  BIOS Data Area                     |
+-------------------------------------+ 0x00000500
|  Interrupt Vector Table             |
+-------------------------------------+ 0x00000000
```

### Key Addresses

| Address | Purpose |
|---------|---------|
| 0x00000000 | Start of RAM, IVT |
| 0x00007C00 | Bootloader load address |
| 0x000B8000 | VGA text buffer |
| 0x00100000 | 1MB mark, kernel load address |

## What Happens Before Our Code Runs

### 1. BIOS Initialization

- Tests memory (POST)
- Detects and initializes devices
- Sets up interrupt handlers (INT 10h for video, INT 13h for disk, etc.)
- Searches for bootable device

### 2. Boot Sector Load

- Reads first 512 bytes of boot device
- Loads to 0x7C00
- Checks for boot signature (0x55AA at bytes 510-511)
- Jumps to 0x7C00

### 3. GRUB Stage 1

- Fits in 512 bytes (boot sector)
- Loads GRUB Stage 2 from disk

### 4. GRUB Stage 2

- Full bootloader with menu, filesystem support
- Reads GRUB configuration
- Loads kernel according to Multiboot specification
- Switches to Protected Mode
- Passes control to kernel

## The Kernel's Starting State

When GRUB transfers control to our kernel:

### CPU State
- Protected Mode enabled
- Paging disabled
- Interrupts disabled
- A20 line enabled (access to >1MB)

### Registers
- EAX = Multiboot magic (0x2BADB002)
- EBX = Physical address of Multiboot info structure
- ESP = Undefined (we must set up our own stack)

### Memory
- Kernel loaded at address specified in linker script (1MB)
- GDT set up by GRUB (but we'll replace it)
- IDT not set up (we must do this)

### What GRUB Provides

1. **Multiboot Info Structure**: Contains:
   - Memory map (available/reserved regions)
   - Module information (initramfs location)
   - Framebuffer information (if requested)
   - Boot device information

2. **Protected Mode**: Already switched from Real Mode

3. **A20 Gate**: Enabled for full memory access

4. **Loaded Kernel**: At the address we specified

## From GRUB to kernel_main()

The handoff from GRUB to our kernel:

```nasm
; GRUB jumps here
_start:
    ; EAX = 0x2BADB002 (Multiboot magic)
    ; EBX = Multiboot info pointer

    ; Set up our stack
    mov esp, stack_top

    ; Save Multiboot info before we clobber registers
    push ebx        ; Multiboot info pointer
    push eax        ; Multiboot magic number

    ; Call C kernel
    call kernel_main

    ; Should never return, but if it does...
    cli
.hang:
    hlt
    jmp .hang
```

Then in C:

```c
void kernel_main(uint32_t magic, multiboot_info_t *mboot_info) {
    // Verify we were loaded by a Multiboot-compliant bootloader
    if (magic != 0x2BADB002) {
        // Not loaded correctly
        return;
    }

    // Now we can use mboot_info to learn about our environment
    // ...
}
```

## Summary

1. **BIOS** initializes hardware and loads bootloader
2. **GRUB** switches to Protected Mode and loads our kernel
3. **Kernel** receives control at `_start` in assembly
4. **Assembly stub** sets up stack and calls `kernel_main()`
5. **C code** takes over from here

The beauty of using GRUB and Multiboot is that we don't need to write our own bootloader. GRUB handles the complex Real Mode to Protected Mode transition and provides us with useful information about the system.

---

*Previous: [Chapter 2: Prerequisites and Tools](02_prerequisites.md)*
*Next: [Chapter 4: The Multiboot Specification](04_multiboot.md)*
