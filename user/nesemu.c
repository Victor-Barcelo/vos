/*
 * nesemu - NES emulator for VOS
 * Uses Nofrendo (LGPL)
 * Controls: Arrow keys = D-pad, Z = A, X = B, Enter = Start, Space = Select, Esc = Quit
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

/* Include nofrendo headers */
#include "nofrendo/config.h"
#include "nofrendo/nes/nes.h"
#include "nofrendo/palettes.h"

/* NES screen dimensions */
#define NES_WIDTH  256
#define NES_HEIGHT 240
#define SCALE      3

/* RGBA framebuffer */
static uint32_t framebuffer[NES_WIDTH * SCALE * NES_HEIGHT * SCALE];

/* Video buffer for nofrendo */
static uint8_t vidbuf[NES_SCREEN_PITCH * NES_SCREEN_HEIGHT];

/* RGBA palette (built from NES palette) */
static uint32_t rgba_palette[256];

/* Terminal mode */
static struct termios g_termios_orig;
static bool g_have_termios = false;

/* Global running flag */
static volatile bool running = true;

/* Input state */
static uint8_t pad_state = 0;

/* Key hold counters - keys stay pressed for multiple frames */
#define KEY_HOLD_FRAMES 6
static uint8_t key_hold[8] = {0};  /* A, B, SELECT, START, UP, DOWN, LEFT, RIGHT */

/* Screen position */
static int out_x = 0, out_y = 0;

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

/* Build RGBA palette from NES palette */
/* VOS uses RGBA format: R in bits 0-7, G in 8-15, B in 16-23, A in 24-31 */
#define RGBA(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

static void build_palette(void) {
    const uint8_t *pal = nes_palettes[0];  /* Use nofrendo palette */

    for (int i = 0; i < 64; i++) {
        uint8_t r = pal[i * 3 + 0];
        uint8_t g = pal[i * 3 + 1];
        uint8_t b = pal[i * 3 + 2];
        uint32_t color = RGBA(r, g, b, 255);
        /* Set it up 3 times for sprite priority/BG transparency */
        rgba_palette[i] = color;
        rgba_palette[i + 64] = color;
        rgba_palette[i + 128] = color;
    }

    /* GUI colors */
    for (int i = 0; i < 8; i++) {
        uint8_t r = gui_pal[i * 3 + 0];
        uint8_t g = gui_pal[i * 3 + 1];
        uint8_t b = gui_pal[i * 3 + 2];
        rgba_palette[192 + i] = RGBA(r, g, b, 255);
    }
}

/* Blit callback from nofrendo */
static void nes_blit(uint8_t *buffer) {
    /* Convert indexed buffer to RGBA with scaling */
    const int scaled_width = NES_WIDTH * SCALE;

    for (int y = 0; y < NES_HEIGHT; y++) {
        uint8_t *src = NES_SCREEN_GETPTR(buffer, 0, y);
        for (int x = 0; x < NES_WIDTH; x++) {
            uint32_t color = rgba_palette[src[x] & 0xFF];
            /* Scale pixel */
            for (int sy = 0; sy < SCALE; sy++) {
                for (int sx = 0; sx < SCALE; sx++) {
                    int fb_x = x * SCALE + sx;
                    int fb_y = y * SCALE + sy;
                    framebuffer[fb_y * scaled_width + fb_x] = color;
                }
            }
        }
    }

    /* Display */
    sys_gfx_blit_rgba(out_x, out_y, NES_WIDTH * SCALE, NES_HEIGHT * SCALE, framebuffer);
}

/* Handle keyboard input */
static void handle_input(void) {
    uint8_t buf[16];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));

    /* Process new key presses - set hold counter */
    for (ssize_t i = 0; i < n; i++) {
        uint8_t b = buf[i];

        if (b == 27) {
            /* ESC - check for arrow keys or quit */
            if (i + 2 < n && buf[i+1] == '[') {
                switch (buf[i+2]) {
                    case 'A': key_hold[4] = KEY_HOLD_FRAMES; break; /* UP */
                    case 'B': key_hold[5] = KEY_HOLD_FRAMES; break; /* DOWN */
                    case 'C': key_hold[7] = KEY_HOLD_FRAMES; break; /* RIGHT */
                    case 'D': key_hold[6] = KEY_HOLD_FRAMES; break; /* LEFT */
                }
                i += 2;
            } else {
                /* Plain ESC = quit */
                running = false;
                return;
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
            key_hold[4] = KEY_HOLD_FRAMES; /* UP */
        } else if (b == 's' || b == 'S') {
            key_hold[5] = KEY_HOLD_FRAMES; /* DOWN */
        } else if (b == 'a' || b == 'A') {
            key_hold[6] = KEY_HOLD_FRAMES; /* LEFT */
        } else if (b == 'd' || b == 'D') {
            key_hold[7] = KEY_HOLD_FRAMES; /* RIGHT */
        }
    }

    /* Build pad_state from hold counters */
    pad_state = 0;
    if (key_hold[0] > 0) { pad_state |= NES_PAD_A; key_hold[0]--; }
    if (key_hold[1] > 0) { pad_state |= NES_PAD_B; key_hold[1]--; }
    if (key_hold[2] > 0) { pad_state |= NES_PAD_SELECT; key_hold[2]--; }
    if (key_hold[3] > 0) { pad_state |= NES_PAD_START; key_hold[3]--; }
    if (key_hold[4] > 0) { pad_state |= NES_PAD_UP; key_hold[4]--; }
    if (key_hold[5] > 0) { pad_state |= NES_PAD_DOWN; key_hold[5]--; }
    if (key_hold[6] > 0) { pad_state |= NES_PAD_LEFT; key_hold[6]--; }
    if (key_hold[7] > 0) { pad_state |= NES_PAD_RIGHT; key_hold[7]--; }

    /* Update nofrendo input */
    input_update(0, pad_state);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: nesemu <rom.nes>\n");
        printf("Controls:\n");
        printf("  Arrow keys/WASD = D-pad\n");
        printf("  Z = A, X = B\n");
        printf("  Enter = Start, Space = Select\n");
        printf("  Esc = Quit\n");
        return 1;
    }

    if (sys_screen_is_fb() != 1) {
        printf("nesemu: framebuffer console required\n");
        return 1;
    }

    int fb_w = 0, fb_h = 0;
    if (!get_fb_size(&fb_w, &fb_h)) {
        printf("nesemu: cannot get screen size\n");
        return 1;
    }

    int reserved = reserved_bottom_px();
    if (reserved > 0) fb_h -= reserved;

    int out_w = NES_WIDTH * SCALE;
    int out_h = NES_HEIGHT * SCALE;

    if (fb_w < out_w || fb_h < out_h) {
        printf("nesemu: screen too small (%dx%d, need %dx%d)\n", fb_w, fb_h, out_w, out_h);
        return 1;
    }

    out_x = (fb_w - out_w) / 2;
    out_y = (fb_h - out_h) / 2;

    /* Build RGBA palette */
    build_palette();

    /* Initialize NES */
    nes_t *nes = nes_init(SYS_DETECT, 0, false, NULL);
    if (!nes) {
        printf("nesemu: failed to initialize NES\n");
        return 1;
    }

    /* Connect joypad */
    input_connect(0, NES_JOYPAD);

    /* Load ROM */
    if (nes_loadfile(argv[1]) < 0) {
        printf("nesemu: failed to load '%s'\n", argv[1]);
        nes_shutdown();
        return 1;
    }

    /* Set video buffer and blit function AFTER loading ROM */
    nes = nes_getptr();  /* Get fresh pointer to global nes state */
    nes_setvidbuf(vidbuf);
    nes->blit_func = nes_blit;

    raw_mode_begin();
    sys_gfx_clear(0);

    /* Main loop */
    uint32_t frame_time = 1000 / 60;  /* ~16ms per frame for NTSC */
    uint32_t last_frame = sys_uptime_ms();

    while (running) {
        uint32_t now = sys_uptime_ms();

        /* Handle input */
        handle_input();

        /* Run one frame */
        nes_emulate(true);

        /* Frame timing */
        uint32_t elapsed = sys_uptime_ms() - now;
        if (elapsed < frame_time) {
            sys_sleep(frame_time - elapsed);
        }
        last_frame = now;
    }

    raw_mode_end();
    nes_shutdown();

    printf("\nnesemu: exited\n");
    return 0;
}
