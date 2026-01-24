# Emulators for VOS

Portable emulators written in C that can be integrated into VOS.

---

## Easy to Port (Single File / Minimal Dependencies)

### CHIP-8
- **Complexity:** Trivial (~500 lines)
- **Hardware:** 4KB RAM, 64x32 display, 16 keys
- **Games:** Pong, Tetris, Space Invaders clones

**Recommended:** Write your own - excellent learning project

```c
typedef struct {
    uint8_t memory[4096];
    uint8_t V[16];         // Registers V0-VF
    uint16_t I;            // Index register
    uint16_t pc;           // Program counter
    uint8_t display[64*32];
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint16_t stack[16];
    uint8_t sp;
    uint8_t keys[16];
} Chip8;

void chip8_cycle(Chip8* c) {
    uint16_t opcode = (c->memory[c->pc] << 8) | c->memory[c->pc + 1];
    c->pc += 2;

    switch (opcode & 0xF000) {
        case 0x0000:
            if (opcode == 0x00E0) clear_display(c);
            else if (opcode == 0x00EE) c->pc = c->stack[--c->sp];
            break;
        case 0x1000: c->pc = opcode & 0x0FFF; break;  // Jump
        case 0x6000: c->V[(opcode >> 8) & 0xF] = opcode & 0xFF; break;  // Set Vx
        // ... more opcodes
    }
}
```

### Peanut-GB (Game Boy)
- **URL:** https://github.com/deltabeard/Peanut-GB
- **License:** MIT
- **Complexity:** Single header (~5000 lines)
- **Features:** Good accuracy, no dependencies

```c
#define ENABLE_SOUND 0  // Disable for VOS initially
#define PEANUT_GB_IMPLEMENTATION
#include "peanut_gb.h"

struct gb_s gb;
uint8_t rom[MAX_ROM_SIZE];
uint8_t ram[MAX_RAM_SIZE];

// Load ROM and initialize
load_file("game.gb", rom, sizeof(rom));
gb_init(&gb, rom, gb_rom_read, gb_cart_ram_read, gb_cart_ram_write, &gb, NULL);

// Main loop
while (running) {
    gb_run_frame(&gb);
    // Copy gb.display to VOS framebuffer
    // Handle input
}
```

### LiteNES (NES)
- **URL:** https://github.com/peng-song/LiteNES
- **License:** MIT
- **Complexity:** ~3000 lines
- **Features:** Clean HAL abstraction, easy to port

HAL functions to implement:
```c
// Graphics
void nes_hal_init();
void nes_set_bg_color(int c);
void nes_draw_pixel(int x, int y, int c);
void nes_flush_buf(uint32_t* buf);

// Input (return 1 if pressed)
int nes_key_state(int key);  // 0-7: A,B,Select,Start,Up,Down,Left,Right

// Timing
void wait_for_frame();
```

### InfoNES (NES)
- **URL:** https://github.com/jay-kumogata/InfoNES
- **License:** GPL
- **Complexity:** ~8000 lines
- **Features:** Better compatibility than LiteNES

---

## Moderate Difficulty

### Walnut-CGB (Game Boy Color)
- **URL:** https://github.com/nicktasios/Walnut-CGB
- **License:** MIT
- **Complexity:** ~8000 lines
- **Features:** GBC support, single header style

### FCEUX (NES) - Stripped
- **URL:** https://github.com/TASEmulators/fceux
- **Notes:** Core can be extracted, but complex

### Snes9x (SNES) - Core Only
- **URL:** https://github.com/snes9xgit/snes9x
- **Notes:** Very large, but core is portable C

### Mednafen PSX (PlayStation)
- **Notes:** Too complex for VOS, needs FPU heavily

---

## Simple Console Specs (For Reference)

| Console | CPU | RAM | VRAM | Resolution |
|---------|-----|-----|------|------------|
| CHIP-8 | Interpreter | 4KB | - | 64x32 |
| Game Boy | 4MHz Z80 | 8KB | 8KB | 160x144 |
| NES | 1.79MHz 6502 | 2KB | 2KB | 256x240 |
| Master System | 3.5MHz Z80 | 8KB | 16KB | 256x192 |
| Game Gear | 3.5MHz Z80 | 8KB | 16KB | 160x144 |
| SNES | 3.58MHz 65816 | 128KB | 64KB | 256x224 |
| Genesis | 7.6MHz 68000 | 64KB | 64KB | 320x224 |

---

## Other Retro Emulators

### Atari 2600
- **Stella** - Full featured but large
- **Simple implementations exist** (~2000 lines)

### ZX Spectrum
- **URL:** https://github.com/AltairZ80/AltairZ80
- **Complexity:** Moderate (Z80 + simple video)

### Commodore 64
- **VICE** - Reference but huge
- **Simple implementations:** Need 6502 + SID + VIC-II

### DOS (8086)
- **fake86:** https://github.com/rubbermallet/fake86
- **8086tiny:** https://github.com/adriancable/8086tiny (~4000 lines!)

### CP/M (Z80)
- **RunCPM:** https://github.com/MockbaTheBorg/RunCPM
- **Simple:** Z80 + minimal I/O

---

## CPU Cores (For Building Emulators)

### Z80 (Game Boy, Master System, Spectrum)
- **URL:** https://github.com/floooh/chips
- **License:** zlib
- **Features:** Cycle-accurate, single header

```c
#define CHIPS_IMPL
#include "chips/z80.h"

z80_t cpu;
z80_init(&cpu);

// Memory read/write callbacks
uint8_t mem_read(void* user, uint16_t addr) {
    return memory[addr];
}

void mem_write(void* user, uint16_t addr, uint8_t data) {
    memory[addr] = data;
}

// Run
z80_exec(&cpu, cycles);
```

### 6502 (NES, C64, Atari)
- **URL:** https://github.com/floooh/chips
- **Also:** fake6502 (~1000 lines)

```c
#include "chips/m6502.h"

m6502_t cpu;
m6502_init(&cpu);
m6502_exec(&cpu, cycles);
```

### 68000 (Genesis, Amiga)
- **Musashi:** https://github.com/kstenerud/Musashi
- **Complexity:** ~10000 lines

---

## Emulator Integration Template

```c
// emu_platform.h - VOS platform layer

#include <stdint.h>
#include <stdbool.h>

// Video
void platform_init(int width, int height, int scale);
void platform_set_pixel(int x, int y, uint32_t color);
void platform_flip();

// Audio (if supported)
void platform_audio_init(int sample_rate);
void platform_audio_queue(int16_t* samples, int count);

// Input
typedef enum {
    KEY_A, KEY_B, KEY_SELECT, KEY_START,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT
} PlatformKey;

bool platform_key_pressed(PlatformKey key);
bool platform_should_quit();

// Timing
uint32_t platform_get_ticks();
void platform_delay(uint32_t ms);

// File I/O
uint8_t* platform_load_file(const char* path, size_t* size);
void platform_save_file(const char* path, uint8_t* data, size_t size);
```

```c
// emu_platform_vos.c - VOS implementation

#include "emu_platform.h"
#include <syscall.h>

static uint32_t* framebuffer;
static int fb_width, fb_height, scale;

void platform_init(int width, int height, int s) {
    scale = s;
    fb_width = width * scale;
    fb_height = height * scale;
    sys_gfx_mode(fb_width, fb_height, 32);
    framebuffer = malloc(fb_width * fb_height * 4);
}

void platform_set_pixel(int x, int y, uint32_t color) {
    // Scale up
    for (int sy = 0; sy < scale; sy++) {
        for (int sx = 0; sx < scale; sx++) {
            int fx = x * scale + sx;
            int fy = y * scale + sy;
            framebuffer[fy * fb_width + fx] = color;
        }
    }
}

void platform_flip() {
    sys_gfx_blit_rgba(framebuffer, 0, 0, fb_width, fb_height);
    sys_gfx_flip();
}

bool platform_key_pressed(PlatformKey key) {
    // Map to VOS keyboard scancodes
    static const int keymap[] = {
        [KEY_A] = 'z',
        [KEY_B] = 'x',
        [KEY_SELECT] = KEY_SHIFT,
        [KEY_START] = KEY_ENTER,
        [KEY_UP] = KEY_UP,
        [KEY_DOWN] = KEY_DOWN,
        [KEY_LEFT] = KEY_LEFT,
        [KEY_RIGHT] = KEY_RIGHT,
    };
    return sys_key_pressed(keymap[key]);
}
```

---

## Recommended Emulators for VOS

| Priority | Emulator | Why |
|----------|----------|-----|
| 1 | **CHIP-8** | Trivial, great test |
| 2 | **Peanut-GB** | Single header, good games |
| 3 | **LiteNES** | Clean code, NES library |
| 4 | **8086tiny** | Run DOS programs! |

---

## Game ROM Sources (Legal)

- **Homebrew:** https://itch.io/ (filter by Game Boy, NES, etc.)
- **PD ROMs:** https://pdroms.de/
- **CHIP-8:** Many public domain games

---

## See Also

- [game_resources.md](game_resources.md) - Game development libraries
- [data_formats.md](data_formats.md) - ROM/save file formats
