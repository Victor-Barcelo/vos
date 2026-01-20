#include "screen.h"
#include "io.h"
#include "serial.h"
#include "string.h"
#include "multiboot.h"
#include "font8x8.h"

typedef enum {
    SCREEN_BACKEND_VGA_TEXT = 0,
    SCREEN_BACKEND_FRAMEBUFFER = 1,
} screen_backend_t;

// VGA text buffer address
static uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;

static screen_backend_t backend = SCREEN_BACKEND_VGA_TEXT;

static int screen_cols_value = VGA_WIDTH;
static int screen_rows_value = VGA_HEIGHT;

// Current cursor position (in text cells)
static int cursor_x = 0;
static int cursor_y = 0;

// Cursor rendering state
static bool cursor_enabled = true;
static int cursor_drawn_x = -1;
static int cursor_drawn_y = -1;

// Current color attribute (VGA-style: fg | (bg << 4))
static uint8_t current_color = 0x0F; // White on black

static int reserved_bottom_rows = 0;

// Framebuffer mode state
#define FONT_W 8
#define FONT_H 8
#define FB_MAX_COLS 200
#define FB_MAX_ROWS 100

static uint8_t* fb_addr = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint8_t fb_bpp = 0;
static uint8_t fb_bytes_per_pixel = 0;
static uint8_t fb_type = 0;
static uint8_t fb_r_pos = 0;
static uint8_t fb_r_size = 0;
static uint8_t fb_g_pos = 0;
static uint8_t fb_g_size = 0;
static uint8_t fb_b_pos = 0;
static uint8_t fb_b_size = 0;

static uint16_t fb_cells[FB_MAX_COLS * FB_MAX_ROWS];

static const uint8_t vga_palette_rgb[16][3] = {
    {0, 0, 0},       // 0 black
    {0, 0, 170},     // 1 blue
    {0, 170, 0},     // 2 green
    {0, 170, 170},   // 3 cyan
    {170, 0, 0},     // 4 red
    {170, 0, 170},   // 5 magenta
    {170, 85, 0},    // 6 brown
    {170, 170, 170}, // 7 light grey
    {85, 85, 85},    // 8 dark grey
    {85, 85, 255},   // 9 light blue
    {85, 255, 85},   // 10 light green
    {85, 255, 255},  // 11 light cyan
    {255, 85, 85},   // 12 light red
    {255, 85, 255},  // 13 light magenta
    {255, 255, 85},  // 14 yellow
    {255, 255, 255}, // 15 white
};

static int usable_height(void) {
    int h = screen_rows_value - reserved_bottom_rows;
    if (h < 1) {
        h = 1;
    }
    return h;
}

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

int screen_cols(void) {
    return screen_cols_value;
}

int screen_rows(void) {
    return screen_rows_value;
}

static void vga_hw_cursor_update(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void vga_hw_cursor_set_enabled(bool enabled) {
    outb(0x3D4, 0x0A);
    uint8_t cur_start = inb(0x3D5);
    if (enabled) {
        cur_start &= (uint8_t)~0x20;
    } else {
        cur_start |= 0x20;
    }
    outb(0x3D4, 0x0A);
    outb(0x3D5, cur_start);
}

static uint32_t fb_pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t value = 0;

    if (fb_r_size) {
        uint32_t mask = (fb_r_size >= 32) ? 0xFFFFFFFFu : ((1u << fb_r_size) - 1u);
        uint32_t component = (uint32_t)r;
        if (fb_r_size < 8) component >>= (8u - fb_r_size);
        value |= (component & mask) << fb_r_pos;
    }
    if (fb_g_size) {
        uint32_t mask = (fb_g_size >= 32) ? 0xFFFFFFFFu : ((1u << fb_g_size) - 1u);
        uint32_t component = (uint32_t)g;
        if (fb_g_size < 8) component >>= (8u - fb_g_size);
        value |= (component & mask) << fb_g_pos;
    }
    if (fb_b_size) {
        uint32_t mask = (fb_b_size >= 32) ? 0xFFFFFFFFu : ((1u << fb_b_size) - 1u);
        uint32_t component = (uint32_t)b;
        if (fb_b_size < 8) component >>= (8u - fb_b_size);
        value |= (component & mask) << fb_b_pos;
    }

    return value;
}

static uint32_t fb_color_from_vga(uint8_t idx) {
    idx &= 0x0F;
    uint8_t r = vga_palette_rgb[idx][0];
    uint8_t g = vga_palette_rgb[idx][1];
    uint8_t b = vga_palette_rgb[idx][2];
    return fb_pack_rgb(r, g, b);
}

static void fb_put_pixel(uint32_t x, uint32_t y, uint32_t pixel) {
    uint8_t* p = fb_addr + y * fb_pitch + x * (uint32_t)fb_bytes_per_pixel;
    switch (fb_bytes_per_pixel) {
        case 4:
            *(uint32_t*)p = pixel;
            break;
        case 3:
            p[0] = (uint8_t)(pixel & 0xFFu);
            p[1] = (uint8_t)((pixel >> 8) & 0xFFu);
            p[2] = (uint8_t)((pixel >> 16) & 0xFFu);
            break;
        case 2:
            *(uint16_t*)p = (uint16_t)(pixel & 0xFFFFu);
            break;
        default:
            *p = (uint8_t)(pixel & 0xFFu);
            break;
    }
}

static void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pixel) {
    for (uint32_t yy = 0; yy < h; yy++) {
        uint8_t* row = fb_addr + (y + yy) * fb_pitch;
        for (uint32_t xx = 0; xx < w; xx++) {
            uint8_t* p = row + (x + xx) * (uint32_t)fb_bytes_per_pixel;
            switch (fb_bytes_per_pixel) {
                case 4:
                    *(uint32_t*)p = pixel;
                    break;
                case 3:
                    p[0] = (uint8_t)(pixel & 0xFFu);
                    p[1] = (uint8_t)((pixel >> 8) & 0xFFu);
                    p[2] = (uint8_t)((pixel >> 16) & 0xFFu);
                    break;
                case 2:
                    *(uint16_t*)p = (uint16_t)(pixel & 0xFFFFu);
                    break;
                default:
                    *p = (uint8_t)(pixel & 0xFFu);
                    break;
            }
        }
    }
}

static void fb_render_cell(int x, int y) {
    if (x < 0 || y < 0 || x >= screen_cols_value || y >= screen_rows_value) {
        return;
    }

    uint16_t entry = fb_cells[y * screen_cols_value + x];
    uint8_t ch = (uint8_t)(entry & 0xFFu);
    uint8_t color = (uint8_t)((entry >> 8) & 0xFFu);
    uint8_t fg = (uint8_t)(color & 0x0Fu);
    uint8_t bg = (uint8_t)((color >> 4) & 0x0Fu);

    uint32_t fg_px = fb_color_from_vga(fg);
    uint32_t bg_px = fb_color_from_vga(bg);

    uint32_t base_x = (uint32_t)x * FONT_W;
    uint32_t base_y = (uint32_t)y * FONT_H;

    for (uint32_t row = 0; row < 8; row++) {
        uint8_t bits = font8x8_basic[ch < 128 ? ch : (uint8_t)'?'][row];
        for (uint32_t col = 0; col < 8; col++) {
            uint32_t px = base_x + col;
            uint32_t py = base_y + row;
            uint32_t pixel = (bits & (1u << col)) ? fg_px : bg_px;
            fb_put_pixel(px, py, pixel);
        }
    }
}

static void fb_draw_cursor_overlay(int x, int y) {
    if (x < 0 || y < 0 || x >= screen_cols_value || y >= usable_height()) {
        return;
    }

    uint16_t entry = fb_cells[y * screen_cols_value + x];
    uint8_t color = (uint8_t)((entry >> 8) & 0xFFu);
    uint8_t fg = (uint8_t)(color & 0x0Fu);
    uint32_t fg_px = fb_color_from_vga(fg);

    uint32_t base_x = (uint32_t)x * FONT_W;
    uint32_t base_y = (uint32_t)y * FONT_H;
    uint32_t y0 = base_y + (FONT_H - 1u);
    fb_fill_rect(base_x, y0, FONT_W, 1u, fg_px);
}

static void fb_update_cursor(void) {
    if (!cursor_enabled) {
        if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
            fb_render_cell(cursor_drawn_x, cursor_drawn_y);
            cursor_drawn_x = -1;
            cursor_drawn_y = -1;
        }
        return;
    }

    if (cursor_drawn_x != cursor_x || cursor_drawn_y != cursor_y) {
        if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
            fb_render_cell(cursor_drawn_x, cursor_drawn_y);
        }
        cursor_drawn_x = cursor_x;
        cursor_drawn_y = cursor_y;
    }

    fb_draw_cursor_overlay(cursor_x, cursor_y);
}

static void update_cursor(void) {
    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        fb_update_cursor();
    } else {
        vga_hw_cursor_update();
    }
}

static void vga_scroll(void) {
    int height = usable_height();

    // Move all lines up by one
    for (int y = 0; y < height - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = VGA_BUFFER[(y + 1) * VGA_WIDTH + x];
        }
    }

    // Clear the last line
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(height - 1) * VGA_WIDTH + x] = vga_entry(' ', current_color);
    }

    cursor_y = height - 1;
}

static void fb_scroll(void) {
    int height = usable_height();
    if (height <= 1) {
        return;
    }

    int cols = screen_cols_value;
    size_t row_bytes = (size_t)cols * sizeof(uint16_t);
    memcpy(&fb_cells[0], &fb_cells[cols], row_bytes * (size_t)(height - 1));

    uint16_t blank = vga_entry(' ', current_color);
    for (int x = 0; x < cols; x++) {
        fb_cells[(height - 1) * cols + x] = blank;
    }

    uint32_t usable_px_height = (uint32_t)height * FONT_H;
    uint32_t copy_bytes = (usable_px_height - FONT_H) * fb_pitch;
    memcpy(fb_addr, fb_addr + FONT_H * fb_pitch, copy_bytes);

    uint8_t bg = (uint8_t)((current_color >> 4) & 0x0Fu);
    uint32_t bg_px = fb_color_from_vga(bg);
    fb_fill_rect(0, (uint32_t)(height - 1) * FONT_H, fb_width, FONT_H, bg_px);

    cursor_y = height - 1;
}

void screen_clear(void) {
    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        uint16_t blank = vga_entry(' ', current_color);
        int cols = screen_cols_value;
        int rows = screen_rows_value;

        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                fb_cells[y * cols + x] = blank;
            }
        }

        uint8_t bg = (uint8_t)((current_color >> 4) & 0x0Fu);
        uint32_t bg_px = fb_color_from_vga(bg);
        fb_fill_rect(0, 0, fb_width, fb_height, bg_px);
    } else {
        for (int y = 0; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', current_color);
            }
        }
    }

    cursor_x = 0;
    cursor_y = 0;
    cursor_drawn_x = -1;
    cursor_drawn_y = -1;
    update_cursor();
}

void screen_init(uint32_t multiboot_magic, uint32_t* mboot_info) {
    current_color = vga_color(VGA_WHITE, VGA_BLACK);
    reserved_bottom_rows = 0;
    cursor_x = 0;
    cursor_y = 0;
    cursor_enabled = true;
    cursor_drawn_x = -1;
    cursor_drawn_y = -1;

    backend = SCREEN_BACKEND_VGA_TEXT;
    screen_cols_value = VGA_WIDTH;
    screen_rows_value = VGA_HEIGHT;

    fb_addr = 0;
    fb_pitch = 0;
    fb_width = 0;
    fb_height = 0;
    fb_bpp = 0;
    fb_bytes_per_pixel = 0;
    fb_type = 0;

    if (multiboot_magic == MULTIBOOT_BOOTLOADER_MAGIC && mboot_info) {
        const multiboot_info_t* mbi = (const multiboot_info_t*)mboot_info;
        if (mbi->flags & (1u << 12)) {
            if (mbi->framebuffer_type == 1 && (mbi->framebuffer_bpp == 32 || mbi->framebuffer_bpp == 24 || mbi->framebuffer_bpp == 16)) {
                uint32_t addr_high = mbi->framebuffer_addr_high;
                uint32_t addr_low = mbi->framebuffer_addr_low;
                if (addr_high == 0 && addr_low != 0) {
                    fb_addr = (uint8_t*)addr_low;
                    fb_pitch = mbi->framebuffer_pitch;
                    fb_width = mbi->framebuffer_width;
                    fb_height = mbi->framebuffer_height;
                    fb_bpp = mbi->framebuffer_bpp;
                    fb_bytes_per_pixel = (uint8_t)((fb_bpp + 7u) / 8u);
                    fb_type = mbi->framebuffer_type;
                    fb_r_pos = mbi->framebuffer_red_field_position;
                    fb_r_size = mbi->framebuffer_red_mask_size;
                    fb_g_pos = mbi->framebuffer_green_field_position;
                    fb_g_size = mbi->framebuffer_green_mask_size;
                    fb_b_pos = mbi->framebuffer_blue_field_position;
                    fb_b_size = mbi->framebuffer_blue_mask_size;

                    int cols = (int)(fb_width / FONT_W);
                    int rows = (int)(fb_height / FONT_H);
                    if (cols < 1) cols = 1;
                    if (rows < 1) rows = 1;
                    if (cols > FB_MAX_COLS) cols = FB_MAX_COLS;
                    if (rows > FB_MAX_ROWS) rows = FB_MAX_ROWS;

                    screen_cols_value = cols;
                    screen_rows_value = rows;
                    backend = SCREEN_BACKEND_FRAMEBUFFER;

                    serial_write_string("[OK] Framebuffer ");
                    serial_write_dec((int32_t)fb_width);
                    serial_write_char('x');
                    serial_write_dec((int32_t)fb_height);
                    serial_write_string("x");
                    serial_write_dec((int32_t)fb_bpp);
                    serial_write_char('\n');
                }
            }
        }
    }

    screen_clear();
}

void screen_putchar(char c) {
    int height = usable_height();

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
                fb_cells[cursor_y * screen_cols_value + cursor_x] = vga_entry(' ', current_color);
                fb_render_cell(cursor_x, cursor_y);
            } else {
                VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', current_color);
            }
        }
    } else {
        if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
            fb_cells[cursor_y * screen_cols_value + cursor_x] = vga_entry(c, current_color);
            fb_render_cell(cursor_x, cursor_y);
        } else {
            VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, current_color);
        }
        cursor_x++;
    }

    // Handle line wrap
    if (cursor_x >= screen_cols_value) {
        cursor_x = 0;
        cursor_y++;
    }

    // Handle scrolling
    if (cursor_y >= height) {
        if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
            fb_scroll();
        } else {
            vga_scroll();
        }
    }

    update_cursor();

    // Mirror VGA output to serial for debugging/logging.
    serial_write_char(c);
}

void screen_print(const char* str) {
    while (*str) {
        screen_putchar(*str++);
    }
}

void screen_println(const char* str) {
    screen_print(str);
    screen_putchar('\n');
}

void screen_print_hex(uint32_t num) {
    screen_print("0x");
    char hex_chars[] = "0123456789ABCDEF";
    bool leading = true;

    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (num >> i) & 0xF;
        if (nibble != 0 || !leading || i == 0) {
            screen_putchar(hex_chars[nibble]);
            leading = false;
        }
    }
}

void screen_print_dec(int32_t num) {
    if (num < 0) {
        screen_putchar('-');
        num = -num;
    }

    if (num == 0) {
        screen_putchar('0');
        return;
    }

    char buffer[12];
    int i = 0;

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i > 0) {
        screen_putchar(buffer[--i]);
    }
}

void screen_set_color(uint8_t fg, uint8_t bg) {
    current_color = vga_color(fg, bg);
}

void screen_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    update_cursor();
}

int screen_get_cursor_x(void) {
    return cursor_x;
}

int screen_get_cursor_y(void) {
    return cursor_y;
}

void screen_backspace(void) {
    if (cursor_x > 0) {
        cursor_x--;
        if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
            fb_cells[cursor_y * screen_cols_value + cursor_x] = vga_entry(' ', current_color);
            fb_render_cell(cursor_x, cursor_y);
        } else {
            VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', current_color);
        }
        update_cursor();
    }
}

void screen_set_reserved_bottom_rows(int rows) {
    if (rows < 0) {
        rows = 0;
    }
    if (rows >= screen_rows_value) {
        rows = screen_rows_value - 1;
    }
    reserved_bottom_rows = rows;
    if (cursor_y >= usable_height()) {
        cursor_y = usable_height() - 1;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_x >= screen_cols_value) cursor_x = 0;
        update_cursor();
    }
}

void screen_write_char_at(int x, int y, char c, uint8_t color) {
    if (x < 0 || x >= screen_cols_value || y < 0 || y >= screen_rows_value) {
        return;
    }
    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        fb_cells[y * screen_cols_value + x] = vga_entry(c, color);
        fb_render_cell(x, y);
        if (cursor_drawn_x == x && cursor_drawn_y == y) {
            cursor_drawn_x = -1;
            cursor_drawn_y = -1;
            update_cursor();
        }
    } else {
        VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(c, color);
    }
}

void screen_write_string_at(int x, int y, const char* str, uint8_t color) {
    if (!str || y < 0 || y >= screen_rows_value) {
        return;
    }
    int col = x;
    while (*str && col < screen_cols_value) {
        if (col >= 0) {
            screen_write_char_at(col, y, *str, color);
        }
        col++;
        str++;
    }
}

void screen_fill_row(int y, char c, uint8_t color) {
    if (y < 0 || y >= screen_rows_value) {
        return;
    }
    for (int x = 0; x < screen_cols_value; x++) {
        screen_write_char_at(x, y, c, color);
    }
}

void screen_cursor_set_enabled(bool enabled) {
    cursor_enabled = enabled;

    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        update_cursor();
        return;
    }

    vga_hw_cursor_set_enabled(enabled);
}
