# Adding Emoji Support to VOS

This document explains how to add true color emoji rendering to VOS using various approaches.

## Table of Contents

1. [Overview](#overview)
2. [Option 1: SSFN (Scalable Screen Font)](#option-1-ssfn-scalable-screen-font)
3. [Option 2: Emoji Sprite Sheets](#option-2-emoji-sprite-sheets)
4. [Option 3: Unicode Symbols in PSF2 Fonts](#option-3-unicode-symbols-in-psf2-fonts)
5. [Font Resources](#font-resources)
6. [Technical Details](#technical-details)
7. [Current VOS Integration](#current-vos-integration)

---

## Overview

True emoji (U+1F600 and above) require special handling because:
- They exist outside the Basic Multilingual Plane (BMP)
- They are typically rendered as color bitmaps, not monochrome glyphs
- Standard console fonts (PSF2) only support ~512-65536 monochrome glyphs

### Approaches Comparison

| Approach | Color Support | Complexity | File Size | Dependencies |
|----------|--------------|------------|-----------|--------------|
| SSFN | Yes | Medium | ~1-5MB | None (header-only) |
| Sprite Sheets | Yes | High | ~5-20MB | PNG decoder |
| PSF2 Unicode | No (monochrome) | Low | ~35KB | None |

---

## Option 1: SSFN (Scalable Screen Font)

**Recommended approach** - Single header C library designed for hobby OS kernels.

### Repository

- **GitLab (Primary)**: https://gitlab.com/bztsrc/scalable-font2
- **Codeberg (Mirror)**: https://codeberg.org/bzt/scalable-font2
- **OSDev Wiki**: https://wiki.osdev.org/Scalable_Screen_Font

### Features

- Single ANSI C header file (`ssfn.h`)
- No FPU/SSE required - works before FPU initialization
- Two renderers:
  - **Simple**: <2KB code, 1 function, zero dependencies
  - **Normal**: ~28KB, scaling, anti-aliasing, color emoji
- MIT licensed
- Supports mixing bitmap and vector glyphs
- Color pixmap support for emoji

### Installation

```bash
# Download ssfn.h
curl -sL "https://gitlab.com/bztsrc/scalable-font2/-/raw/master/ssfn.h" \
    -o /path/to/vos/third_party/ssfn/ssfn.h
```

### Usage - Simple Console Renderer

```c
#define SSFN_CONSOLEBITMAP_TRUECOLOR
#include "ssfn.h"

// Set up font and framebuffer
ssfn_src = &_binary_font_sfn_start;  // Embedded .sfn font
ssfn_dst.ptr = framebuffer;
ssfn_dst.p = pitch;                   // Bytes per scanline
ssfn_dst.w = width;
ssfn_dst.h = height;
ssfn_dst.fg = 0xFFFFFF;               // White
ssfn_dst.bg = 0;                      // Transparent (0 = no fill)
ssfn_dst.x = 0;
ssfn_dst.y = 0;

// Render characters
ssfn_putc('H');
ssfn_putc(0x1F600);  // 😀 emoji
```

### Usage - Full Renderer

```c
#define SSFN_IMPLEMENTATION
#include "ssfn.h"

ssfn_t ctx = { 0 };
ssfn_buf_t buf = {
    .ptr = framebuffer,
    .w = screen_width,
    .h = screen_height,
    .p = pitch,
    .fg = 0xFFFFFFFF,
    .bg = 0xFF000000
};

// Load multiple fonts (regular + emoji)
ssfn_load(&ctx, &_binary_font_sfn_start);
ssfn_load(&ctx, &_binary_emoji_sfn_start);

// Select and render
ssfn_select(&ctx, SSFN_FAMILY_MONOSPACE, NULL, SSFN_STYLE_REGULAR, 16);
ssfn_render(&ctx, &buf, "Hello 😀 World!");

ssfn_free(&ctx);
```

### Converting Fonts to SSFN Format

The `sfnconv` tool converts various font formats to SSFN:

```bash
# Clone and build sfnconv
git clone https://gitlab.com/bztsrc/scalable-font2.git
cd scalable-font2/sfnconv
make

# Convert TTF/OTF fonts
./sfnconv -s 16 input.ttf output-16.sfn
./sfnconv -s 32 input.ttf output-32.sfn

# Convert with color preservation (for emoji)
./sfnconv -c input.ttf output.sfn

# Convert PSF2 to SSFN
./sfnconv input.psf output.sfn
```

### Dependencies for sfnconv

```bash
# Ubuntu/Debian
sudo apt install libfreetype-dev libimagequant-dev
```

### Known Issues with Color Emoji

NotoColorEmoji.ttf uses CBDT/CBLC format which sfnconv may not handle well:

```
libsfn: no layers in font???
Error saving!
```

**Workarounds**:
1. Use COLR/CPAL format emoji fonts (newer format)
2. Convert emoji from PNG sprite sheets instead
3. Use a different emoji font (OpenMoji, Twemoji)

---

## Option 2: Emoji Sprite Sheets

Pre-rendered PNG sprite sheets with coordinate-based lookup.

### Repository

- **emoji-data**: https://github.com/iamcal/emoji-data
- **Twemoji**: https://github.com/twitter/twemoji
- **OpenMoji**: https://github.com/hfg-gmuend/openmoji

### Available Sizes

- 16px, 20px, 32px, 64px (emoji-data)
- 72px (Twemoji SVG-based)
- 72px (OpenMoji)

### Sprite Sheet Format

Each emoji has a 1px transparent border:
- 64px sheet = 66px squares
- 16px sheet = 18px squares

### Coordinate Lookup

```c
// From emoji.json data:
// "sheet_x": 32, "sheet_y": 35

int size = 64;  // or 16, 32, etc.
int x = (sheet_x * (size + 2)) + 1;
int y = (sheet_y * (size + 2)) + 1;
```

### Licensing

| Source | License | Commercial Use |
|--------|---------|----------------|
| Google/Noto | Apache 2.0 | Yes |
| Twitter/Twemoji | CC-BY 4.0 | Yes (with attribution) |
| Apple | Proprietary | No |
| Facebook | Unclear | No |
| OpenMoji | CC-BY-SA 4.0 | Yes (with attribution) |

### Implementation Steps

1. Embed sprite sheet PNG in kernel
2. Implement PNG decoder (or use pre-decoded raw bitmap)
3. Parse emoji.json for codepoint → coordinate mapping
4. Detect emoji codepoints in text rendering
5. Blit sprite rectangle to framebuffer

---

## Option 3: Unicode Symbols in PSF2 Fonts

Use monochrome Unicode symbols as emoji alternatives.

### Fonts with Good Unicode Coverage

| Font | Characters | Sizes | Notes |
|------|------------|-------|-------|
| Cozette | 6,078 | 13x13, 26x26 | Excellent symbol coverage |
| Unifont | 65,536+ | Various | Full BMP coverage |
| Spleen | ~1,000 | 5x8 to 32x64 | Box drawing, Braille |
| Terminus | ~1,000 | 6x12 to 32x16 | Clean, readable |

### Available Symbols (BMP Range)

These render in standard PSF2 fonts:

```
Smileys:     ☺ ☻ ♥ ♦ ♣ ♠
Weather:     ☀ ☁ ☂ ☃ ★ ☆
Arrows:      → ← ↑ ↓ ↔ ↕
Math:        ± × ÷ ≠ ≤ ≥ ∞
Music:       ♩ ♪ ♫ ♬
Chess:       ♔ ♕ ♖ ♗ ♘ ♙ ♚ ♛ ♜ ♝ ♞ ♟
Misc:        ✓ ✗ ✔ ✘ ● ○ ◆ ◇
Box Drawing: ┌ ┐ └ ┘ ─ │ ├ ┤ ┬ ┴ ┼
Blocks:      █ ▄ ▀ ▌ ▐ ░ ▒ ▓
```

### Cozette Font (Recommended)

```bash
# Already included in VOS at:
# third_party/fonts/cozette/cozette.psf (13x13)
# third_party/fonts/cozette/cozette_hidpi.psf (26x26)
```

Download: https://github.com/slavfox/Cozette

---

## Font Resources

### SSFN Fonts

| Font | URL |
|------|-----|
| SSFN Repository | https://gitlab.com/bztsrc/scalable-font2 |
| SSFN Documentation | https://gitlab.com/bztsrc/scalable-font2/-/blob/master/docs/API.md |
| Font Format Spec | https://gitlab.com/bztsrc/scalable-font2/-/blob/master/docs/sfn_format.md |

### Emoji Fonts (TTF/OTF)

| Font | URL | Format |
|------|-----|--------|
| Noto Color Emoji | https://github.com/googlefonts/noto-emoji | CBDT/CBLC |
| Noto Emoji (B&W) | https://github.com/googlefonts/noto-emoji | Vector |
| Twemoji | https://github.com/twitter/twemoji | SVG/PNG |
| OpenMoji | https://github.com/hfg-gmuend/openmoji | SVG/PNG |

### PSF2 Console Fonts

| Font | URL |
|------|-----|
| Cozette | https://github.com/slavfox/Cozette |
| Spleen | https://github.com/fcambus/spleen |
| Terminus | http://terminus-font.sourceforge.net/ |
| Unifont | https://unifoundry.com/unifont/ |
| Tamzen | https://github.com/sunaku/tamzen-font |
| Gohufont | https://github.com/koemaeda/gohufont-ttf |

### Sprite Sheets

| Source | URL |
|--------|-----|
| emoji-data | https://github.com/iamcal/emoji-data |
| Twemoji Assets | https://github.com/twitter/twemoji/tree/master/assets |
| OpenMoji PNG | https://github.com/hfg-gmuend/openmoji/releases |

### Tools

| Tool | URL | Purpose |
|------|-----|---------|
| sfnconv | https://gitlab.com/bztsrc/scalable-font2/-/tree/master/sfnconv | Convert fonts to SSFN |
| psftools | https://www.seasip.info/Unix/PSF/ | PSF font manipulation |
| fontforge | https://fontforge.org/ | Font editing |
| ttf2psf | https://github.com/nickvanbenschoten/ttf2psf | TTF to PSF conversion |

---

## Technical Details

### Emoji Unicode Ranges

```
U+2600-U+26FF    Miscellaneous Symbols
U+2700-U+27BF    Dingbats
U+1F300-U+1F5FF  Miscellaneous Symbols and Pictographs
U+1F600-U+1F64F  Emoticons
U+1F680-U+1F6FF  Transport and Map Symbols
U+1F700-U+1F77F  Alchemical Symbols
U+1F900-U+1F9FF  Supplemental Symbols and Pictographs
U+1FA00-U+1FA6F  Chess Symbols
U+1FA70-U+1FAFF  Symbols and Pictographs Extended-A
U+1FB00-U+1FBFF  Symbols for Legacy Computing
```

### Detecting Emoji in Code

```c
bool is_emoji(uint32_t codepoint) {
    // Miscellaneous Symbols and Dingbats
    if (codepoint >= 0x2600 && codepoint <= 0x27BF) return true;

    // Main emoji blocks (U+1F000+)
    if (codepoint >= 0x1F000 && codepoint <= 0x1FFFF) return true;

    // Variation selectors
    if (codepoint == 0xFE0F || codepoint == 0xFE0E) return true;

    return false;
}
```

### Color Font Formats

| Format | Description | Support |
|--------|-------------|---------|
| CBDT/CBLC | Embedded PNG bitmaps | Android, Chrome |
| COLR/CPAL | Layered color vectors | Windows 10+, macOS |
| SVG | Embedded SVG graphics | Firefox, Safari |
| sbix | Apple's bitmap format | macOS, iOS |

---

## Current VOS Integration

### Files Added

```
third_party/ssfn/
├── ssfn.h              # SSFN header (download from GitLab)
├── setup_ssfn.sh       # Setup script
├── README.md           # Integration guide
├── emoji/
│   └── NotoColorEmoji.ttf
└── sfnconv             # Font converter tool

kernel/
├── ssfn_render.h       # SSFN integration header
└── ssfn_render.c       # SSFN integration implementation
```

### Build Integration

The Makefile includes:
- SSFN header path in CFLAGS: `-I$(THIRD_PARTY_DIR)/ssfn`
- ssfn_render.c is compiled with other kernel sources

### Initialization

In `screen.c`, SSFN is initialized when framebuffer mode is enabled:

```c
ssfn_render_init(fb_addr, (int)fb_width, (int)fb_height, (int)fb_pitch);
```

### Future Work

1. Successfully convert color emoji font to SSFN format
2. Embed converted emoji font in kernel
3. Modify `screen_put_codepoint()` to use SSFN for emoji ranges
4. Add emoji font selection to `font` program
5. Consider sprite sheet approach as fallback

---

## References

- [OSDev Wiki - Scalable Screen Font](https://wiki.osdev.org/Scalable_Screen_Font)
- [PC Screen Font - Wikipedia](https://en.wikipedia.org/wiki/PC_Screen_Font)
- [Unicode Emoji Charts](https://unicode.org/emoji/charts/full-emoji-list.html)
- [Implementation of Emoji - Wikipedia](https://en.wikipedia.org/wiki/Implementation_of_emoji)
- [Linux Console Fonts - Arch Wiki](https://wiki.archlinux.org/title/Linux_console#Fonts)
