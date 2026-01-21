#ifndef VOS_BASIC_SCREEN_H
#define VOS_BASIC_SCREEN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "syscall.h"

static inline void screen_clear(void) {
    // ANSI clear + home.
    fputs("\x1b[2J\x1b[H", stdout);
    fflush(stdout);
}

static inline void screen_cursor_set_enabled(bool enabled) {
    (void)enabled;
}

static inline bool screen_is_framebuffer(void) {
    return sys_screen_is_fb() == 1;
}

static inline bool screen_graphics_clear(uint8_t bg_vga) {
    return sys_gfx_clear((uint32_t)bg_vga) == 0;
}

static inline bool screen_graphics_putpixel(int32_t x, int32_t y, uint8_t vga_color) {
    return sys_gfx_pset(x, y, (uint32_t)vga_color) == 0;
}

static inline bool screen_graphics_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t vga_color) {
    return sys_gfx_line(x0, y0, x1, y1, (uint32_t)vga_color) == 0;
}

#endif

