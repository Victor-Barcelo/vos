# SSFN (Scalable Screen Font) Integration for VOS

This directory contains the SSFN library for rendering scalable fonts including color emoji in VOS.

## Quick Setup

```bash
cd /home/victor/www/vos/third_party/ssfn
chmod +x setup_ssfn.sh
./setup_ssfn.sh
```

This will:
1. Download the full `ssfn.h` header
2. Download Noto Color Emoji font
3. Build the `sfnconv` tool
4. Convert fonts to SSFN format

## Manual Setup

If the script fails, run these commands manually:

```bash
# Download ssfn.h
curl -sL "https://gitlab.com/bztsrc/scalable-font2/-/raw/master/ssfn.h" -o ssfn.h

# Download Noto Color Emoji
mkdir -p emoji
curl -sL "https://github.com/googlefonts/noto-emoji/raw/main/fonts/NotoColorEmoji.ttf" -o emoji/NotoColorEmoji.ttf
```

## Using SSFN in VOS Kernel

### Option 1: Simple Console Renderer (Recommended for VOS)

The simple renderer is designed for OS consoles - just one function, <2KB code, zero dependencies:

```c
#define SSFN_CONSOLEBITMAP_TRUECOLOR
#include "ssfn.h"

// Initialize
ssfn_src = &_binary_font_sfn_start;  // Your embedded .sfn font
ssfn_dst.ptr = framebuffer;           // Framebuffer pointer
ssfn_dst.p = pitch;                   // Bytes per scanline
ssfn_dst.fg = 0xFFFFFF;               // Foreground color (white)
ssfn_dst.bg = 0x000000;               // Background color (black)
ssfn_dst.x = 0;                       // X position
ssfn_dst.y = 0;                       // Y position

// Render a character (returns advance width)
ssfn_putc('H');
ssfn_putc('i');

// Render a string
while (*s) ssfn_putc(ssfn_utf8(&s));
```

### Option 2: Full Renderer (For emoji and scaling)

```c
#define SSFN_IMPLEMENTATION
#include "ssfn.h"

ssfn_t ctx = { 0 };
ssfn_buf_t buf = {
    .ptr = framebuffer,
    .w = screen_width,
    .h = screen_height,
    .p = pitch,
    .x = 0,
    .y = 0,
    .fg = 0xFFFFFFFF,
    .bg = 0xFF000000
};

// Load fonts (can load multiple)
ssfn_load(&ctx, &_binary_font_sfn_start);
ssfn_load(&ctx, &_binary_emoji_sfn_start);

// Select font
ssfn_select(&ctx, SSFN_FAMILY_MONOSPACE, NULL, SSFN_STYLE_REGULAR, 16);

// Render text (handles UTF-8 including emoji)
ssfn_render(&ctx, &buf, "Hello 😀 World! 🎉");

// Clean up
ssfn_free(&ctx);
```

## Converting Fonts

Use `sfnconv` to convert TTF/OTF/PSF fonts to SSFN format:

```bash
# Convert at specific size (e.g., 16, 24, 32 pixels)
./sfnconv/sfnconv -s 16 input.ttf output-16.sfn
./sfnconv/sfnconv -s 32 input.ttf output-32.sfn

# Convert with color preservation (for emoji)
./sfnconv/sfnconv -c input.ttf output.sfn
```

## Embedding Fonts in VOS Kernel

1. Add to Makefile:
```makefile
SSFN_FONTS = \
    $(THIRD_PARTY_DIR)/ssfn/converted/DejaVuSansMono-16.sfn \
    $(THIRD_PARTY_DIR)/ssfn/converted/emoji-32.sfn

SSFN_FONT_OBJS = $(patsubst %.sfn,$(BUILD_DIR)/ssfn/%.o,$(notdir $(SSFN_FONTS)))
```

2. Add extern declarations in your kernel code:
```c
extern const uint8_t _binary_DejaVuSansMono_16_sfn_start[];
extern const uint8_t _binary_DejaVuSansMono_16_sfn_end[];
extern const uint8_t _binary_emoji_32_sfn_start[];
extern const uint8_t _binary_emoji_32_sfn_end[];
```

## SSFN vs PSF2 Comparison

| Feature | PSF2 | SSFN |
|---------|------|------|
| Max glyphs | 65536 | Unlimited |
| Color emoji | No | Yes |
| Scalable | No (bitmap only) | Yes (vector + bitmap) |
| File size | Small | Larger |
| Dependencies | None | None (simple) / memset,realloc (full) |
| Code size | N/A | <2KB (simple) / ~28KB (full) |

## Resources

- [SSFN Repository](https://gitlab.com/bztsrc/scalable-font2)
- [SSFN Documentation](https://gitlab.com/bztsrc/scalable-font2/-/blob/master/docs/API.md)
- [OSDev Wiki](https://wiki.osdev.org/Scalable_Screen_Font)
- [Font Format Spec](https://gitlab.com/bztsrc/scalable-font2/-/blob/master/docs/sfn_format.md)

## License

SSFN is MIT licensed. Noto Color Emoji is Apache 2.0 licensed.
