#include "screen.h"
#include "io.h"

// VGA text buffer address
static uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;

// Current cursor position
static int cursor_x = 0;
static int cursor_y = 0;

// Current color attribute
static uint8_t current_color = 0x0F; // White on black

// Create a VGA entry
static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

// Create a color attribute
static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

// Update hardware cursor position
static void update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// Scroll the screen up by one line
static void screen_scroll(void) {
    // Move all lines up by one
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = VGA_BUFFER[(y + 1) * VGA_WIDTH + x];
        }
    }

    // Clear the last line
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', current_color);
    }

    cursor_y = VGA_HEIGHT - 1;
}

void screen_init(void) {
    current_color = vga_color(VGA_WHITE, VGA_BLACK);
    screen_clear();
}

void screen_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', current_color);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void screen_putchar(char c) {
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
            VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', current_color);
        }
    } else {
        VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, current_color);
        cursor_x++;
    }

    // Handle line wrap
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    // Handle scrolling
    if (cursor_y >= VGA_HEIGHT) {
        screen_scroll();
    }

    update_cursor();
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
        VGA_BUFFER[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', current_color);
        update_cursor();
    }
}
