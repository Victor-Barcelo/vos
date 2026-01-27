#ifndef SCREEN_H
#define SCREEN_H

#include "types.h"

// VGA text mode colors
enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15,
};

// Screen dimensions
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Initialize the screen
void screen_init(uint32_t multiboot_magic, uint32_t* mboot_info);

// Current screen text dimensions (in character cells)
int screen_cols(void);
int screen_rows(void);
int screen_usable_rows(void);

// Clear the screen
void screen_clear(void);

// Print a single character
void screen_putchar(char c);

// Print a string
void screen_print(const char* str);

// Print a string with newline
void screen_println(const char* str);

// Print a hexadecimal number
void screen_print_hex(uint32_t num);

// Print a decimal number
void screen_print_dec(int32_t num);

// Set the text color
void screen_set_color(uint8_t fg, uint8_t bg);

// Move cursor to position
void screen_set_cursor(int x, int y);

// Get current cursor position
int screen_get_cursor_x(void);
int screen_get_cursor_y(void);

// Backspace - remove last character
void screen_backspace(void);

// Reserve bottom rows from scrolling/output (e.g. status bar).
void screen_set_reserved_bottom_rows(int rows);

// Write directly to the VGA buffer without moving the cursor or scrolling.
void screen_write_char_at(int x, int y, char c, uint8_t color);
void screen_write_string_at(int x, int y, const char* str, uint8_t color);
void screen_fill_row(int y, char c, uint8_t color);
void screen_fill_row_full(int y, char c, uint8_t color);  // Fills entire pixel row including margins

// Batch mode for flicker-free updates (write all cells, then render once).
void screen_write_char_at_batch(int x, int y, char c, uint8_t color);
void screen_render_row(int y);
void screen_render_row_noclear(int y);  // For status bar - no clear first

// Enable/disable the VGA hardware cursor.
void screen_cursor_set_enabled(bool enabled);

// Framebuffer backend information (returns 0 when not in framebuffer mode).
bool screen_is_framebuffer(void);
uint32_t screen_framebuffer_width(void);
uint32_t screen_framebuffer_height(void);
uint32_t screen_framebuffer_bpp(void);
uint32_t screen_font_width(void);
uint32_t screen_font_height(void);

// Framebuffer font registry (only available in framebuffer text console mode).
typedef struct screen_font_info {
    char name[32];
    uint32_t width;
    uint32_t height;
} screen_font_info_t;

int screen_font_count(void);
int screen_font_get_current(void);
int screen_font_get_info(int index, screen_font_info_t* out);
int screen_font_set(int index);

// Color theme support
int screen_theme_count(void);
int screen_theme_get_current(void);
int screen_theme_get_info(int index, char* name_out, uint32_t name_cap);
int screen_theme_set(int index);

// Refresh screen (redraw all cells with current colors)
void screen_refresh(void);

// Simple framebuffer pixel primitives (no-op in VGA text mode).
bool screen_graphics_clear(uint8_t bg_vga);
bool screen_graphics_putpixel(int32_t x, int32_t y, uint8_t vga_color);
bool screen_graphics_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t vga_color);
bool screen_graphics_blit_rgba(int32_t x, int32_t y, uint32_t w, uint32_t h, const uint8_t* rgba, uint32_t stride_bytes);

// Double buffering support for flicker-free graphics.
// When enabled, all drawing goes to an off-screen buffer.
// Call screen_gfx_flip() to copy the buffer to the visible screen.
bool screen_gfx_set_double_buffering(bool enabled);
bool screen_gfx_is_double_buffered(void);
bool screen_gfx_flip(void);

// Simple scrollback support (framebuffer text console only).
bool screen_scrollback_active(void);
void screen_scrollback_lines(int32_t delta);
void screen_scrollback_reset(void);

// Mouse cursor overlay (framebuffer text console only).
void screen_mouse_set_enabled(bool enabled);
void screen_mouse_set_pos(int x, int y);

// VT100 mouse reporting state (set via CSI ?1000 h/l and CSI ?1006 h/l).
bool screen_vt_mouse_reporting_enabled(void);
bool screen_vt_mouse_reporting_sgr(void);
bool screen_vt_mouse_reporting_wheel(void);

// Virtual console support (Alt+1/2/3/4 to switch).
void screen_console_init(void);
int screen_console_count(void);
int screen_console_active(void);
void screen_console_switch(int console);

// Dump current screen text content (for remote debugging/MCP server).
// Writes the visible text content to serial port as plain text.
// Returns number of characters written.
int screen_dump_to_serial(void);

#endif
