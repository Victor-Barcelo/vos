# Chapter 38: Font System and Themes

VOS features a comprehensive font system supporting multiple PSF2 fonts, runtime font switching, and color themes. This enables users to customize their console experience based on resolution and personal preference.

## Overview

The font system provides:

1. **42 built-in fonts** - From 5x8 to 32x64 pixels
2. **Runtime font switching** - Change fonts without rebooting
3. **Auto font selection** - Optimal font for screen resolution
4. **16 color themes** - Predefined color palettes
5. **Persistent preferences** - Saved across reboots

```
+------------------+
|  User Program    |
|  (font, theme)   |
+------------------+
        |
        v
+------------------+
|   Syscall API    |
| - sys_font_set   |
| - sys_theme_set  |
+------------------+
        |
        v
+------------------+
|   Screen Driver  |
| - font_registry  |
| - theme_palette  |
| - cell_buffer    |
+------------------+
        |
        v
+------------------+
|   Framebuffer    |
| - pixel_render   |
+------------------+
```

## PSF2 Font Format

VOS uses PSF2 (PC Screen Font version 2), a standard bitmap font format.

### PSF2 Header

```c
#define PSF2_MAGIC 0x864ab572

typedef struct {
    uint32_t magic;          // PSF2_MAGIC
    uint32_t version;        // 0
    uint32_t header_size;    // Size of header
    uint32_t flags;          // 0 or 1 (has unicode table)
    uint32_t num_glyphs;     // Number of glyphs (usually 256 or 512)
    uint32_t bytes_per_glyph;// Size of each glyph
    uint32_t height;         // Glyph height in pixels
    uint32_t width;          // Glyph width in pixels
} psf2_header_t;
```

### Glyph Data Layout

Glyphs are stored as packed bitmaps:

```
For an 8x16 font (8 pixels wide, 16 rows):
- 1 byte per row (8 bits = 8 pixels)
- 16 bytes per glyph
- Total: 256 glyphs * 16 bytes = 4096 bytes

For a 16x32 font (16 pixels wide, 32 rows):
- 2 bytes per row (16 bits = 16 pixels)
- 64 bytes per glyph
- Total: 256 glyphs * 64 bytes = 16384 bytes

Bit order: MSB = leftmost pixel
```

### Rendering Algorithm

```c
void fb_render_glyph(uint8_t c, int x, int y, uint32_t fg, uint32_t bg) {
    const psf2_header_t* hdr = current_font->header;
    const uint8_t* glyph = current_font->data +
                           hdr->header_size +
                           c * hdr->bytes_per_glyph;

    uint32_t bytes_per_row = (hdr->width + 7) / 8;

    for (uint32_t row = 0; row < hdr->height; row++) {
        for (uint32_t col = 0; col < hdr->width; col++) {
            uint32_t byte_idx = row * bytes_per_row + col / 8;
            uint32_t bit_idx = 7 - (col % 8);

            bool pixel_on = (glyph[byte_idx] >> bit_idx) & 1;
            uint32_t color = pixel_on ? fg : bg;

            fb_putpixel(x + col, y + row, color);
        }
    }
}
```

## Font Registry

VOS maintains a registry of available fonts:

### Font Entry Structure

```c
typedef struct screen_font_info {
    char name[32];       // Human-readable name
    uint32_t width;      // Glyph width in pixels
    uint32_t height;     // Glyph height in pixels
} screen_font_info_t;

// Internal font structure
typedef struct {
    const uint8_t* data;    // Raw PSF2 data
    uint32_t size;          // Data size
    bool unicode;           // Has unicode table
} font_entry_t;
```

### Built-in Fonts

VOS includes **42 built-in fonts** for different resolutions and styles:

| Font Family | Sizes Available | Best For |
|-------------|-----------------|----------|
| VGA | 8x16, 28x16 | Legacy/Low-res |
| Terminus | 8x16, 12x24, 14x28, 16x32, 18x36, 20x40, 24x48 | General use |
| Cozette | 13x6 | Compact displays |
| Cozette HiDPI | 26x12 | High-DPI displays |
| Lat2-Terminus | 16x16 | International |
| Unifont | 16x16 | Unicode coverage |
| Spleen | 5x8, 6x12, 8x16, 12x24, 16x32, 32x64 | Modern look |
| Tamzen | 7x14 | Clean aesthetic |
| Gohufont | 11x11, 14x14 | Retro style |
| Iosevka | 8x17, 11x24 | Programming |
| Dina | 8x16 | Readability |
| Creep | 11x4 | Minimal |
| SSFN converted | Various | Extended chars |

Use `font list` to see all 42 available fonts with their exact dimensions.

### Font Selection by Resolution

```c
void screen_select_optimal_font(uint32_t width, uint32_t height) {
    // Calculate target cell count
    // Aim for ~80 columns for comfortable reading

    if (width >= 1920) {
        // Large fonts for high-DPI
        screen_font_set(FONT_TERMINUS_16x32);
    } else if (width >= 1280) {
        screen_font_set(FONT_TERMINUS_12x24);
    } else if (width >= 1024) {
        screen_font_set(FONT_UNIFONT_16x16);
    } else {
        // Default VGA-style
        screen_font_set(FONT_TERMINUS_8x16);
    }
}
```

## Font Syscalls

### Available Syscalls

```c
// Get number of available fonts
int sys_font_count(void);

// Get currently active font index
int sys_font_get_current(void);

// Get font information
int sys_font_info(uint32_t index, vos_font_info_t* out);

// Set active font (requires recalculating screen dimensions)
int sys_font_set(uint32_t index);
```

### Userspace Wrappers

```c
// In user/syscall.h
typedef struct vos_font_info {
    char name[32];
    uint32_t width;
    uint32_t height;
} vos_font_info_t;

static inline int sys_font_count(void) {
    int ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(SYS_FONT_COUNT));
    return ret;
}

static inline int sys_font_set(uint32_t idx) {
    int ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(SYS_FONT_SET), "b"(idx));
    return ret;
}
```

### Kernel Implementation

```c
// In kernel/syscall.c
case SYS_FONT_COUNT:
    frame->eax = (uint32_t)screen_font_count();
    return frame;

case SYS_FONT_GET_CURRENT:
    frame->eax = (uint32_t)screen_font_get_current();
    return frame;

case SYS_FONT_SET: {
    int idx = (int)frame->ebx;
    int rc = screen_font_set(idx);
    frame->eax = (uint32_t)rc;
    return frame;
}
```

## The `font` Utility

VOS includes an interactive font selector built with termbox2:

### Usage

```bash
font            # Interactive TUI selector
font list       # List available fonts
font set 2      # Set font by index
font set Terminus-16x32  # Set by name
font save       # Save current font as default
font load       # Load saved default font
```

### Interactive Interface

```
================================================================================
                    VOS Font Selector  |  Press 'q' quit, Tab cycle views
================================================================================
Up/Down: Navigate   a/Enter: Apply   s: Save default   q: Quit

  #  Name                      Size
--------------------------------------------------------------------------------
  0  Terminus-8x16             8x16
* 1  Terminus-12x24            12x24   <- current
  2  Terminus-16x32            16x32
  3  Unifont-16x16             16x16
  4  VGA-8x16                  8x16

                                                                         ^^^
Applied: Terminus-12x24
================================================================================
 Font 2/5 | Active: Terminus-12x24
================================================================================
```

### Key Implementation Details

The font program uses termbox2 for a flicker-free UI:

```c
// Initialize termbox
int rc = tb_init();
if (rc != 0) {
    fprintf(stderr, "font: failed to initialize termbox\n");
    return 1;
}

// Main loop
while (running) {
    draw_ui(infos, count, sel, cur, scroll_offset, message);

    struct tb_event ev;
    rc = tb_poll_event(&ev);

    if (ev.type == TB_EVENT_KEY) {
        if (ev.key == TB_KEY_ENTER || ev.ch == 'a') {
            // Apply selected font
            sys_font_set(sel);
            cur = sel;

            // Font change affects screen size - reinit termbox
            tb_shutdown();
            tb_init();
        }
    }
}

tb_shutdown();
```

### Persistent Font Settings

Font preference is saved to `/disk/fontrc`:

```c
#define FONT_CONFIG_FILE "/disk/fontrc"

static int save_default_font(int font_idx) {
    FILE* f = fopen(FONT_CONFIG_FILE, "w");
    if (!f) return -1;
    fprintf(f, "%d\n", font_idx);
    fclose(f);
    return 0;
}

static int load_default_font(void) {
    FILE* f = fopen(FONT_CONFIG_FILE, "r");
    if (!f) return -1;

    int font_idx = -1;
    fscanf(f, "%d", &font_idx);
    fclose(f);
    return font_idx;
}
```

Auto-loading in `/etc/profile`:

```bash
# Load user's default font (if saved)
font load 2>/dev/null
```

## Color Themes

VOS supports multiple color themes for the console.

### VGA Color Palette

The base palette uses standard VGA colors:

```c
// Standard 16-color VGA palette
static const uint32_t vga_palette_default[16] = {
    0x000000,  // 0: Black
    0x0000AA,  // 1: Blue
    0x00AA00,  // 2: Green
    0x00AAAA,  // 3: Cyan
    0xAA0000,  // 4: Red
    0xAA00AA,  // 5: Magenta
    0xAA5500,  // 6: Brown
    0xAAAAAA,  // 7: Light Gray
    0x555555,  // 8: Dark Gray
    0x5555FF,  // 9: Light Blue
    0x55FF55,  // 10: Light Green
    0x55FFFF,  // 11: Light Cyan
    0xFF5555,  // 12: Light Red
    0xFF55FF,  // 13: Light Magenta
    0xFFFF55,  // 14: Yellow
    0xFFFFFF   // 15: White
};
```

### Theme Structure

```c
typedef struct color_theme {
    char name[32];
    uint32_t palette[16];
    uint8_t default_fg;
    uint8_t default_bg;
} color_theme_t;
```

### Available Themes

VOS includes **16 color themes**:

| Theme | Description |
|-------|-------------|
| Default | Standard VGA colors |
| Solarized Dark | Popular dark theme |
| Solarized Light | Light variant |
| Monokai | Vibrant programmer theme |
| Dracula | Dark purple theme |
| Nord | Arctic blue theme |
| Gruvbox Dark | Retro groove dark |
| Gruvbox Light | Retro groove light |
| One Dark | Atom editor theme |
| Tokyo Night | Modern dark theme |
| Catppuccin | Pastel dark theme |
| Zenburn | Low-contrast dark |
| Tomorrow Night | Subdued colors |
| Ayu Dark | Elegant dark |
| Ayu Light | Clean light |
| Material | Material design colors |

Use `theme list` to see all 16 themes.

### Theme Syscalls

```c
int sys_theme_count(void);
int sys_theme_get_current(void);
int sys_theme_get_info(uint32_t idx, char* name, uint32_t cap);
int sys_theme_set(uint32_t idx);
```

### The `theme` Utility

```bash
theme            # Interactive selector
theme list       # List available themes
theme set 2      # Set by index
theme set Nord   # Set by name
```

## Screen Refresh on Font/Theme Change

When the font or theme changes, the screen must be refreshed:

```c
int screen_font_set(int index) {
    if (index < 0 || index >= font_count) {
        return -EINVAL;
    }

    // Switch to new font
    current_font = &fonts[index];

    // Recalculate screen dimensions
    screen_cols_value = fb_width / current_font->width;
    screen_rows_value = fb_height / current_font->height;

    // Reallocate cell buffer if needed
    if (cell_buffer) {
        kfree(cell_buffer);
    }
    cell_buffer = kcalloc(screen_cols_value * screen_rows_value,
                          sizeof(cell_t));

    // Clear and redraw
    screen_clear();

    return 0;
}

void screen_refresh(void) {
    // Redraw all cells with current colors
    for (int y = 0; y < screen_rows_value; y++) {
        for (int x = 0; x < screen_cols_value; x++) {
            render_cell(x, y);
        }
    }
}
```

## Adding Custom Fonts

### Converting a TTF Font

Use `psf2` tools to convert TrueType fonts:

```bash
# Install fontforge
apt install fontforge

# Convert TTF to PSF2
fontforge -script ttf2psf.pe MyFont.ttf MyFont.psf
```

### Embedding in VOS

1. Convert font to C array:

```bash
xxd -i MyFont.psf > font_myfont.c
```

2. Add to font registry:

```c
// In kernel/screen.c
extern const uint8_t font_myfont[];
extern const uint32_t font_myfont_len;

static void init_font_registry(void) {
    // ... existing fonts ...

    fonts[font_count++] = (font_entry_t){
        .data = font_myfont,
        .size = font_myfont_len,
        .unicode = false
    };
}
```

3. Add to Makefile:

```makefile
FONT_SOURCES = kernel/fonts/font_myfont.c
```

## Unicode Support

### Basic Multilingual Plane

Fonts with unicode tables can map codepoints to glyphs:

```c
// Unicode table follows glyph data
// Format: sequences of [codepoint, 0xFF] or [codepoint, codepoint, 0xFF]

uint32_t find_glyph_for_codepoint(uint32_t cp) {
    if (!current_font->unicode) {
        // ASCII-only font
        return (cp < 256) ? cp : '?';
    }

    const uint8_t* unicode_table = current_font->data +
                                   header->header_size +
                                   header->num_glyphs * header->bytes_per_glyph;

    // Search unicode table for mapping
    // ... implementation ...
}
```

### Unifont

VOS includes GNU Unifont which covers:
- Basic Latin
- Latin Extended
- Greek
- Cyrillic
- Many other scripts
- Over 57,000 glyphs

## Performance Considerations

### Cell Buffer

VOS uses a cell buffer to avoid redundant rendering:

```c
typedef struct cell {
    uint32_t codepoint;   // Character
    uint8_t fg;           // Foreground color
    uint8_t bg;           // Background color
    uint8_t attr;         // Bold, underline, etc.
    bool dirty;           // Needs redraw
} cell_t;

void screen_putchar_at(int x, int y, char c, uint8_t fg, uint8_t bg) {
    cell_t* cell = &cell_buffer[y * screen_cols_value + x];

    // Only redraw if changed
    if (cell->codepoint != c || cell->fg != fg || cell->bg != bg) {
        cell->codepoint = c;
        cell->fg = fg;
        cell->bg = bg;
        cell->dirty = true;
    }
}
```

### Batch Rendering

For flicker-free updates:

```c
void screen_write_char_at_batch(int x, int y, char c, uint8_t color) {
    // Update cell buffer only, don't render yet
    cell_t* cell = &cell_buffer[y * screen_cols_value + x];
    cell->codepoint = c;
    cell->fg = color & 0x0F;
    cell->bg = (color >> 4) & 0x0F;
    cell->dirty = true;
}

void screen_render_row(int y) {
    // Render all dirty cells in row
    for (int x = 0; x < screen_cols_value; x++) {
        cell_t* cell = &cell_buffer[y * screen_cols_value + x];
        if (cell->dirty) {
            render_cell(x, y);
            cell->dirty = false;
        }
    }
}
```

## Summary

The VOS font system provides:

1. **42 PSF2 fonts** for different resolutions and styles
2. **Runtime switching** via syscalls and utilities
3. **16 color themes** for personalization
4. **Persistent settings** saved to `/disk/fontrc`
5. **Unicode support** via Unifont
6. **Optimized rendering** with cell buffering

This enables a customizable, modern console experience while maintaining simplicity.

---

*Previous: [Chapter 37: Sound Blaster 16 Audio](37_sound_blaster.md)*
*Next: [Chapter 39: User Management](39_users.md)*
