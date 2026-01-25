/*
 * gbemu - Game Boy emulator for VOS
 * Uses Peanut-GB (MIT License)
 * Controls: Arrow keys = D-pad, Z = A, X = B, Enter = Start, Shift = Select, Esc = Quit
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <syscall.h>

/* Peanut-GB configuration */
#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#define PEANUT_GB_IS_LITTLE_ENDIAN 1
#define PEANUT_GB_USE_INTRINSICS 0

#define PEANUT_GB_IMPLEMENTATION
#include "peanut-gb/peanut_gb.h"

/* Game Boy screen dimensions */
#define GB_WIDTH  160
#define GB_HEIGHT 144
#define SCALE     4

/* RGBA framebuffer */
static uint32_t framebuffer[GB_WIDTH * SCALE * GB_HEIGHT * SCALE];

/* ROM and save data */
static uint8_t *rom_data = NULL;
static size_t rom_size = 0;
static uint8_t *cart_ram = NULL;
static size_t cart_ram_size = 0;

/* Color palette (classic green) */
static const uint32_t palette[4] = {
    0xFF9BBC0F,  /* Lightest */
    0xFF8BAC0F,
    0xFF306230,
    0xFF0F380F   /* Darkest */
};

/* Emulator context */
static struct gb_s gb;

/* Key hold counters - keys stay pressed for multiple frames */
#define KEY_HOLD_FRAMES 6
static uint8_t key_hold[8] = {0};  /* A, B, SELECT, START, RIGHT, LEFT, UP, DOWN */

/* Terminal mode */
static struct termios g_termios_orig;
static bool g_have_termios = false;

static bool get_fb_size(int *out_w, int *out_h) {
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != 0) return false;
    if (ws.ws_xpixel == 0 || ws.ws_ypixel == 0) return false;
    *out_w = (int)ws.ws_xpixel;
    *out_h = (int)ws.ws_ypixel;
    return true;
}

static int reserved_bottom_px(void) {
    int idx = sys_font_get_current();
    if (idx < 0) return 0;
    vos_font_info_t info;
    if (sys_font_info((uint32_t)idx, &info) != 0) return 0;
    return (int)info.height;
}

static void raw_mode_begin(void) {
    if (tcgetattr(STDIN_FILENO, &g_termios_orig) == 0) {
        g_have_termios = true;
        struct termios raw = g_termios_orig;
        cfmakeraw(&raw);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    write(STDOUT_FILENO, "\033[?25l", 6);  /* Hide cursor */
}

static void raw_mode_end(void) {
    write(STDOUT_FILENO, "\033[?25h", 6);  /* Show cursor */
    if (g_have_termios) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_termios_orig);
    }
}

/* ROM read callback */
static uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
    (void)gb;
    if (addr < rom_size) return rom_data[addr];
    return 0xFF;
}

/* Cart RAM read callback */
static uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
    (void)gb;
    if (cart_ram && addr < cart_ram_size) return cart_ram[addr];
    return 0xFF;
}

/* Cart RAM write callback */
static void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t val) {
    (void)gb;
    if (cart_ram && addr < cart_ram_size) cart_ram[addr] = val;
}

/* Error callback */
static void gb_error(struct gb_s *gb, const enum gb_error_e err, const uint16_t addr) {
    (void)gb;
    (void)err;
    (void)addr;
    /* Silently ignore errors for now */
}

/* LCD line draw callback */
static void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line) {
    (void)gb;
    const int scaled_width = GB_WIDTH * SCALE;

    for (int x = 0; x < GB_WIDTH; x++) {
        uint8_t shade = pixels[x] & 0x03;
        uint32_t color = palette[shade];

        /* Scale 2x */
        for (int sy = 0; sy < SCALE; sy++) {
            for (int sx = 0; sx < SCALE; sx++) {
                int fb_x = x * SCALE + sx;
                int fb_y = line * SCALE + sy;
                framebuffer[fb_y * scaled_width + fb_x] = color;
            }
        }
    }
}

/* Load ROM file */
static bool load_rom(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("gbemu: cannot open '%s'\n", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    rom_size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    if (rom_size < 0x8000) {
        printf("gbemu: ROM too small\n");
        fclose(f);
        return false;
    }

    rom_data = malloc(rom_size);
    if (!rom_data) {
        printf("gbemu: out of memory\n");
        fclose(f);
        return false;
    }

    if (fread(rom_data, 1, rom_size, f) != rom_size) {
        printf("gbemu: read error\n");
        free(rom_data);
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: gbemu <rom.gb>\n");
        printf("Controls:\n");
        printf("  Arrow keys = D-pad\n");
        printf("  Z = A, X = B\n");
        printf("  Enter = Start, Shift = Select\n");
        printf("  Esc = Quit\n");
        return 1;
    }

    if (sys_screen_is_fb() != 1) {
        printf("gbemu: framebuffer console required\n");
        return 1;
    }

    int fb_w = 0, fb_h = 0;
    if (!get_fb_size(&fb_w, &fb_h)) {
        printf("gbemu: cannot get screen size\n");
        return 1;
    }

    int reserved = reserved_bottom_px();
    if (reserved > 0) fb_h -= reserved;

    int out_w = GB_WIDTH * SCALE;
    int out_h = GB_HEIGHT * SCALE;

    if (fb_w < out_w || fb_h < out_h) {
        printf("gbemu: screen too small (%dx%d, need %dx%d)\n", fb_w, fb_h, out_w, out_h);
        return 1;
    }

    int out_x = (fb_w - out_w) / 2;
    int out_y = (fb_h - out_h) / 2;

    /* Load ROM */
    if (!load_rom(argv[1])) {
        return 1;
    }

    /* Initialize emulator */
    enum gb_init_error_e ret = gb_init(&gb, gb_rom_read, gb_cart_ram_read,
                                        gb_cart_ram_write, gb_error, NULL);
    if (ret != GB_INIT_NO_ERROR) {
        printf("gbemu: init error %d\n", ret);
        free(rom_data);
        return 1;
    }

    /* Allocate cart RAM if needed */
    cart_ram_size = gb_get_save_size(&gb);
    if (cart_ram_size > 0) {
        cart_ram = calloc(1, cart_ram_size);
    }

    /* Set LCD callback */
    gb.display.lcd_draw_line = lcd_draw_line;

    raw_mode_begin();
    sys_gfx_clear(0);

    /* Main loop */
    uint32_t frame_time = 1000 / 60;  /* ~16ms per frame */
    uint32_t last_frame = sys_uptime_ms();

    while (1) {
        uint32_t now = sys_uptime_ms();

        /* Handle input - read new keys and set hold counters */
        uint8_t buf[8];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

        for (ssize_t i = 0; i < n; i++) {
            uint8_t b = buf[i];

            if (b == 27) {
                /* ESC - check for arrow keys or quit */
                if (i + 2 < n && buf[i+1] == '[') {
                    switch (buf[i+2]) {
                        case 'A': key_hold[6] = KEY_HOLD_FRAMES; break; /* UP */
                        case 'B': key_hold[7] = KEY_HOLD_FRAMES; break; /* DOWN */
                        case 'C': key_hold[4] = KEY_HOLD_FRAMES; break; /* RIGHT */
                        case 'D': key_hold[5] = KEY_HOLD_FRAMES; break; /* LEFT */
                    }
                    i += 2;
                } else {
                    /* Plain ESC = quit */
                    goto done;
                }
            } else if (b == 'z' || b == 'Z') {
                key_hold[0] = KEY_HOLD_FRAMES; /* A */
            } else if (b == 'x' || b == 'X') {
                key_hold[1] = KEY_HOLD_FRAMES; /* B */
            } else if (b == '\r' || b == '\n') {
                key_hold[3] = KEY_HOLD_FRAMES; /* START */
            } else if (b == ' ') {
                key_hold[2] = KEY_HOLD_FRAMES; /* SELECT */
            } else if (b == 'w' || b == 'W') {
                key_hold[6] = KEY_HOLD_FRAMES; /* UP */
            } else if (b == 's' || b == 'S') {
                key_hold[7] = KEY_HOLD_FRAMES; /* DOWN */
            } else if (b == 'a' || b == 'A') {
                key_hold[5] = KEY_HOLD_FRAMES; /* LEFT */
            } else if (b == 'd' || b == 'D') {
                key_hold[4] = KEY_HOLD_FRAMES; /* RIGHT */
            }
        }

        /* Build joypad state from hold counters */
        gb.direct.joypad = 0;  /* 0 = not pressed, 1 = pressed for GB */
        if (key_hold[0] > 0) { gb.direct.joypad_bits.a = 1; key_hold[0]--; }
        if (key_hold[1] > 0) { gb.direct.joypad_bits.b = 1; key_hold[1]--; }
        if (key_hold[2] > 0) { gb.direct.joypad_bits.select = 1; key_hold[2]--; }
        if (key_hold[3] > 0) { gb.direct.joypad_bits.start = 1; key_hold[3]--; }
        if (key_hold[4] > 0) { gb.direct.joypad_bits.right = 1; key_hold[4]--; }
        if (key_hold[5] > 0) { gb.direct.joypad_bits.left = 1; key_hold[5]--; }
        if (key_hold[6] > 0) { gb.direct.joypad_bits.up = 1; key_hold[6]--; }
        if (key_hold[7] > 0) { gb.direct.joypad_bits.down = 1; key_hold[7]--; }

        /* Run one frame */
        gb_run_frame(&gb);

        /* Display */
        sys_gfx_blit_rgba(out_x, out_y, out_w, out_h, framebuffer);

        /* Frame timing */
        uint32_t elapsed = sys_uptime_ms() - now;
        if (elapsed < frame_time) {
            sys_sleep(frame_time - elapsed);
        }
        last_frame = now;
    }

done:
    raw_mode_end();

    /* Cleanup */
    free(rom_data);
    free(cart_ram);

    printf("\ngbemu: exited\n");
    return 0;
}
