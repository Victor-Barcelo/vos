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
void screen_init(void);

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

#endif
