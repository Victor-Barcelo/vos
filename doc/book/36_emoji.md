# Chapter 36: Emoji Support

VOS includes full-color emoji rendering in the framebuffer console, bringing modern Unicode emoji to the text-mode experience. This chapter explains the implementation and how to use emoji in your programs.

## Overview

VOS supports 89 color emoji from the OpenMoji library, rendered as 32x32 pixel sprites with full alpha transparency. Emoji are displayed as double-width characters, taking two text cells horizontally.

## Supported Emoji

### Smileys and Emotions
| Emoji | Codepoint | Name |
|-------|-----------|------|
| 😀 | U+1F600 | grinning face |
| 😁 | U+1F601 | beaming face |
| 😂 | U+1F602 | face with tears of joy |
| 😃 | U+1F603 | grinning face with big eyes |
| 😄 | U+1F604 | grinning face with smiling eyes |
| 😉 | U+1F609 | winking face |
| 😊 | U+1F60A | smiling face with smiling eyes |
| 😎 | U+1F60E | smiling face with sunglasses |
| 😍 | U+1F60D | smiling face with heart-eyes |
| 😘 | U+1F618 | face blowing a kiss |
| 😢 | U+1F622 | crying face |
| 😭 | U+1F62D | loudly crying face |
| 😡 | U+1F621 | pouting face |
| 😱 | U+1F631 | face screaming in fear |
| 🤔 | U+1F914 | thinking face |

### Gestures and People
| Emoji | Codepoint | Name |
|-------|-----------|------|
| 👍 | U+1F44D | thumbs up |
| 👎 | U+1F44E | thumbs down |
| 👏 | U+1F44F | clapping hands |
| 👋 | U+1F44B | waving hand |
| 👌 | U+1F44C | OK hand |
| 🙏 | U+1F64F | folded hands |
| ✌️ | U+270C | victory hand |

### Animals
| Emoji | Codepoint | Name |
|-------|-----------|------|
| 🐶 | U+1F436 | dog face |
| 🐱 | U+1F431 | cat face |
| 🐭 | U+1F42D | mouse face |
| 🐰 | U+1F430 | rabbit face |
| 🐻 | U+1F43B | bear face |
| 🐷 | U+1F437 | pig face |
| 🐸 | U+1F438 | frog |
| 🐵 | U+1F435 | monkey face |
| 🐔 | U+1F414 | chicken |
| 🐧 | U+1F427 | penguin |

### Food and Drink
| Emoji | Codepoint | Name |
|-------|-----------|------|
| 🍔 | U+1F354 | hamburger |
| 🍕 | U+1F355 | pizza |
| 🎂 | U+1F382 | birthday cake |
| ☕ | U+2615 | hot beverage |
| 🍺 | U+1F37A | beer mug |

### Objects and Symbols
| Emoji | Codepoint | Name |
|-------|-----------|------|
| ❤️ | U+2764 | red heart |
| 💻 | U+1F4BB | laptop |
| 🔥 | U+1F525 | fire |
| ⭐ | U+2B50 | star |
| 🎁 | U+1F381 | wrapped gift |
| 🏆 | U+1F3C6 | trophy |
| 💡 | U+1F4A1 | light bulb |
| ✅ | U+2705 | check mark |
| ❌ | U+274C | cross mark |
| ❓ | U+2753 | question mark |
| ❗ | U+2757 | exclamation mark |
| ⚠️ | U+26A0 | warning |
| 🚀 | U+1F680 | rocket |

### Weather and Nature
| Emoji | Codepoint | Name |
|-------|-----------|------|
| ☀️ | U+2600 | sun |
| ☁️ | U+2601 | cloud |
| ☔ | U+2614 | umbrella with rain |
| ⚡ | U+26A1 | lightning |
| 🌈 | U+1F308 | rainbow |

## Implementation Details

### Architecture

The emoji system consists of three main components:

1. **Sprite Data** (`kernel/emoji_data.c`): Auto-generated C arrays containing 32x32 ARGB pixel data for each emoji.

2. **Lookup API** (`include/emoji.h`): Functions to check if a codepoint is an emoji and retrieve sprite data.

3. **Renderer** (`kernel/screen.c`): Integration with the framebuffer text renderer for displaying emoji.

### Rendering Pipeline

When a Unicode codepoint is written to the console:

```
1. screen_put_codepoint() receives codepoint
2. Check: emoji_is_emoji(codepoint)?
3. If yes: emoji_lookup(codepoint) returns sprite data
4. If sprite exists:
   a. Store codepoint in emoji tracking array
   b. Render 32x32 sprite with alpha blending
   c. Mark next cell as continuation
   d. Advance cursor by 2 positions
5. If no sprite: render as normal character
```

### Double-Width Cells

Emoji occupy two character cells horizontally:

```
+---+---+---+---+---+---+
| H | e | l | 😀    | ! |
+---+---+---+---+---+---+
         ^     ^
         |     +-- Continuation marker (0xFFFFFFFF)
         +-------- Emoji codepoint stored here
```

### Alpha Blending

Emoji sprites support full alpha transparency. The renderer performs per-pixel alpha blending:

```c
// For each pixel in the 32x32 sprite:
uint8_t alpha = (pixel >> 24) & 0xFF;
if (alpha == 0) {
    // Fully transparent - skip
} else if (alpha == 255) {
    // Fully opaque - write directly
    framebuffer[offset] = pixel;
} else {
    // Semi-transparent - blend with background
    // result = (src * alpha + dst * (255 - alpha)) / 255
}
```

### Virtual Console Support

Each of the 4 virtual consoles maintains its own emoji state:

```c
typedef struct {
    fb_cell_t cells[FB_MAX_COLS * FB_MAX_ROWS];
    uint32_t emoji_codepoints[FB_MAX_COLS * FB_MAX_ROWS];
    // ...
} virtual_console_t;
```

When switching consoles, emoji are preserved and correctly restored.

## Using Emoji in Programs

### C Programs

Simply print UTF-8 encoded emoji:

```c
#include <stdio.h>

int main(void) {
    printf("Hello! 😀\n");
    printf("Status: ✅ OK\n");
    printf("Warning: ⚠️  Check disk space\n");
    printf("Rocket launch in 3... 2... 1... 🚀\n");
    return 0;
}
```

### Shell

Echo emoji directly (shell supports UTF-8):

```bash
echo "Build successful! ✅"
echo "Error: ❌ File not found"
```

### Example: Progress Indicator

```c
void show_progress(int percent) {
    printf("\r[");
    int filled = percent / 5;
    for (int i = 0; i < 20; i++) {
        if (i < filled) printf("🟩");
        else printf("⬜");
    }
    printf("] %d%% 🚀", percent);
    fflush(stdout);
}
```

## Generating Emoji Data

The emoji sprite data is generated by `tools/emoji_gen.py`:

```bash
cd tools
python3 emoji_gen.py
```

This script:
1. Downloads emoji PNG files from OpenMoji (72x72 pixels)
2. Resizes to 32x32 using PIL
3. Converts to ARGB C arrays
4. Generates `kernel/emoji_data.c` and `include/emoji.h`

### Adding New Emoji

To add more emoji, edit `tools/emoji_gen.py` and add entries to the `EMOJI_LIST`:

```python
EMOJI_LIST = [
    (0x1F600, "grinning"),
    (0x1F4BB, "laptop"),
    # Add new emoji here:
    (0x1F389, "party_popper"),
]
```

Then regenerate:

```bash
python3 emoji_gen.py
make clean && make
```

## Limitations

1. **Framebuffer only**: Emoji rendering requires framebuffer mode. VGA text mode falls back to spaces.

2. **Fixed size**: All emoji are 32x32 pixels. They scale with font size by occupying 2 cells.

3. **No skin tones**: Variation selectors and skin tone modifiers are not supported.

4. **No ZWJ sequences**: Complex emoji like flags or family combinations are not supported.

5. **89 emoji limit**: Only the emoji in the compiled sprite data are available.

## API Reference

### emoji.h

```c
// Check if a Unicode codepoint is a potential emoji
// Returns 1 if in emoji range, 0 otherwise
int emoji_is_emoji(uint32_t codepoint);

// Look up emoji sprite data
// Returns pointer to 32x32 ARGB pixel array, or NULL if not found
const uint32_t* emoji_lookup(uint32_t codepoint);

// Get emoji sprite size (always 32)
int emoji_get_size(void);

// Sprite dimensions
#define EMOJI_SIZE 32
```

## Credits

Emoji artwork from [OpenMoji](https://openmoji.org/) - the open-source emoji project.
Licensed under CC BY-SA 4.0.
