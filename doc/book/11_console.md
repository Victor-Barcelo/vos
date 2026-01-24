# Chapter 11: Console Output

VOS supports two text output backends:

1. **VGA text mode** (legacy): Direct writes to VGA text buffer
2. **Multiboot framebuffer** (modern): Pixel-based rendering with PSF2 fonts

## VGA Text Mode

### VGA Text Buffer

The VGA text buffer is at physical address 0xB8000:

```
+--------------------------------------------------------+
|  Address: 0xB8000                                       |
|  Size: 80 columns x 25 rows x 2 bytes = 4000 bytes     |
|                                                        |
|  Each cell: [Character Byte][Attribute Byte]           |
+--------------------------------------------------------+
```

### Character Cell Format

Each cell is 2 bytes:

```
Byte 0: ASCII character code
Byte 1: Attribute (colors)

Attribute byte:
+-----+-----+-----+-----+-----+-----+-----+-----+
|  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
+-----+-----+-----+-----+-----+-----+-----+-----+
|Blink|   Background    |     Foreground        |
+-----+-----------------+-----------------------+
```

### VGA Colors

```c
enum vga_color {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15
};
```

### Basic VGA Functions

```c
static uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

void vga_putchar_at(char c, uint8_t color, int x, int y) {
    VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(c, color);
}
```

### VGA Hardware Cursor

```c
void vga_set_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;

    outb(0x3D4, 0x0F);              // Cursor location low
    outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E);              // Cursor location high
    outb(0x3D5, (pos >> 8) & 0xFF);
}
```

## Framebuffer Console

When Multiboot provides a framebuffer, VOS uses pixel-based rendering.

### Framebuffer Info

```c
typedef struct {
    uint32_t addr;          // Physical address
    uint32_t pitch;         // Bytes per row
    uint32_t width;         // Width in pixels
    uint32_t height;        // Height in pixels
    uint8_t  bpp;           // Bits per pixel
    uint8_t  red_pos;       // Red bit position
    uint8_t  red_size;      // Red bit size
    uint8_t  green_pos;
    uint8_t  green_size;
    uint8_t  blue_pos;
    uint8_t  blue_size;
} framebuffer_info_t;
```

### Pixel Operations

```c
static inline void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    uint32_t *pixel = (uint32_t *)(fb_addr + y * fb_pitch + x * 4);
    *pixel = color;
}

static uint32_t make_color(uint8_t r, uint8_t g, uint8_t b) {
    return (r << fb_info.red_pos) |
           (g << fb_info.green_pos) |
           (b << fb_info.blue_pos);
}
```

## PSF2 Fonts

VOS uses PSF2 (PC Screen Font version 2) for framebuffer text.

### PSF2 Header

```c
typedef struct {
    uint8_t  magic[4];      // 0x72, 0xb5, 0x4a, 0x86
    uint32_t version;       // Zero
    uint32_t header_size;   // Size of header
    uint32_t flags;         // 0 or 1 (has unicode table)
    uint32_t num_glyphs;    // Number of glyphs
    uint32_t bytes_per_glyph;
    uint32_t height;        // Glyph height in pixels
    uint32_t width;         // Glyph width in pixels
} psf2_header_t;
```

### Rendering a Glyph

```c
void fb_render_glyph(char c, int x, int y, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = font_data + header->header_size +
                           (uint8_t)c * header->bytes_per_glyph;

    for (uint32_t row = 0; row < header->height; row++) {
        for (uint32_t col = 0; col < header->width; col++) {
            uint32_t byte_idx = (row * header->width + col) / 8;
            uint32_t bit_idx = 7 - ((row * header->width + col) % 8);

            uint32_t color = (glyph[byte_idx] & (1 << bit_idx)) ? fg : bg;
            fb_putpixel(x + col, y + row, color);
        }
    }
}
```

### Font Selection

VOS selects font size based on resolution:

```c
void screen_init(uint32_t magic, multiboot_info_t *mboot) {
    if (mboot->framebuffer_width >= 1024) {
        // Use larger Terminus font for high resolution
        font_set(&font_terminus_16);
    } else {
        // Use smaller VGA font
        font_set(&font_vga_8x16);
    }
}
```

## Unified Console API

VOS provides a unified API that works with both backends:

```c
void screen_putchar(char c);
void screen_print(const char *str);
void screen_println(const char *str);
void screen_clear(void);
void screen_set_color(uint8_t fg, uint8_t bg);
int  screen_cols(void);
int  screen_rows(void);
```

### Putchar Implementation

```c
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
            put_cell(' ', cursor_x, cursor_y);
        }
    } else {
        put_cell(c, cursor_x, cursor_y);
        cursor_x++;
    }

    // Line wrap
    if (cursor_x >= screen_cols_value) {
        cursor_x = 0;
        cursor_y++;
    }

    // Scroll if needed
    if (cursor_y >= height) {
        scroll();
        cursor_y = height - 1;
    }

    update_cursor();

    // Mirror to serial for debugging
    serial_write_char(c);
}
```

### Scrolling

```c
static void scroll(void) {
    // Move all lines up by one
    for (int y = 0; y < usable_height() - 1; y++) {
        for (int x = 0; x < screen_cols_value; x++) {
            // Copy cell from (x, y+1) to (x, y)
            copy_cell(x, y + 1, x, y);
        }
    }

    // Clear last line
    for (int x = 0; x < screen_cols_value; x++) {
        put_cell(' ', x, usable_height() - 1);
    }
}
```

## ANSI Escape Sequences

VOS supports ANSI escape sequences for cursor and color control:

### Supported Sequences

| Sequence | Action |
|----------|--------|
| `\e[H` | Home cursor |
| `\e[2J` | Clear screen |
| `\e[K` | Clear to end of line |
| `\e[nA` | Cursor up n |
| `\e[nB` | Cursor down n |
| `\e[nC` | Cursor forward n |
| `\e[nD` | Cursor back n |
| `\e[y;xH` | Move cursor to y,x |
| `\e[0m` | Reset attributes |
| `\e[1m` | Bold |
| `\e[30-37m` | Foreground color |
| `\e[40-47m` | Background color |
| `\e[?25l` | Hide cursor |
| `\e[?25h` | Show cursor |

### Escape Sequence Parser

```c
static enum { STATE_NORMAL, STATE_ESC, STATE_CSI } parse_state;
static char csi_buf[16];
static int csi_len;

void screen_putchar(char c) {
    switch (parse_state) {
    case STATE_NORMAL:
        if (c == '\e') {
            parse_state = STATE_ESC;
        } else {
            output_char(c);
        }
        break;

    case STATE_ESC:
        if (c == '[') {
            parse_state = STATE_CSI;
            csi_len = 0;
        } else {
            parse_state = STATE_NORMAL;
        }
        break;

    case STATE_CSI:
        if (c >= 0x40 && c <= 0x7E) {
            // Final byte - execute sequence
            csi_buf[csi_len] = '\0';
            execute_csi(csi_buf, c);
            parse_state = STATE_NORMAL;
        } else if (csi_len < sizeof(csi_buf) - 1) {
            csi_buf[csi_len++] = c;
        }
        break;
    }
}
```

## Status Bar

VOS reserves the bottom row for a status bar:

```c
static int reserved_bottom_rows = 1;

void statusbar_update(void) {
    rtc_datetime_t dt;
    rtc_read_datetime(&dt);

    char buf[80];
    snprintf(buf, sizeof(buf),
             " %04d-%02d-%02d %02d:%02d | Up: %us | Mem: %uMB ",
             dt.year, dt.month, dt.day, dt.hour, dt.minute,
             timer_uptime_ms() / 1000,
             pmm_free_frames() * 4 / 1024);

    // Draw on bottom row with inverted colors
    int y = screen_rows_value - 1;
    for (int x = 0; x < screen_cols_value && buf[x]; x++) {
        put_cell_color(buf[x], x, y, VGA_BLUE, VGA_WHITE);
    }
}
```

## Serial Mirroring

All console output is mirrored to the serial port:

```c
void screen_putchar(char c) {
    // ... render to screen ...

    // Mirror to serial
    serial_write_char(c);
}
```

This provides:
- Debug output even if screen fails
- Log capture via QEMU `-serial file:log.txt`
- Consistent logging during development

## Summary

VOS console output provides:

1. **VGA text mode** for legacy/fallback display
2. **Framebuffer mode** for high-resolution graphics
3. **PSF2 fonts** for scalable text rendering
4. **ANSI sequences** for cursor and color control
5. **Status bar** for system information
6. **Serial mirroring** for debugging

The unified API allows code to work regardless of which backend is active.

---

*Previous: [Chapter 10: Timekeeping](10_timekeeping.md)*
*Next: [Chapter 12: Keyboard Driver](12_keyboard.md)*
