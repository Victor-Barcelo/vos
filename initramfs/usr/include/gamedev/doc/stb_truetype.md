# stb_truetype.h - TrueType Font Parsing and Rendering

## Overview

**stb_truetype.h** is a single-header library for parsing TrueType font files and rendering glyphs to bitmaps. It supports font loading, glyph metrics extraction, shape extraction, and antialiased rendering including SDF (Signed Distance Field) output.

## Source

- **Original Repository:** https://github.com/nothings/stb
- **File:** stb_truetype.h
- **Version:** 1.26
- **Author:** Sean Barrett (2009-2021)

## License

This software is dual-licensed:
- **Public Domain** (www.unlicense.org)
- **MIT License**

Choose whichever you prefer.

## Security Warning

**NO SECURITY GUARANTEE - DO NOT USE ON UNTRUSTED FONT FILES**

This library does no range checking of offsets found in font files. An attacker can use it to read arbitrary memory.

## Features

- Parse TrueType (.ttf) and OpenType (.otf) files
- TrueType Collection (.ttc) support
- Extract glyph metrics (advance, bearings)
- Extract glyph shapes (curves, lines)
- Render glyphs to 8-bit antialiased bitmaps
- Signed Distance Field (SDF) rendering
- Font texture atlas packing (with stb_rect_pack.h)
- Kerning support (including GPOS)
- Subpixel positioning
- Oversampling for improved quality

## API Reference

### Compile-Time Options

| Macro | Description |
|-------|-------------|
| `STB_TRUETYPE_IMPLEMENTATION` | Define in ONE file to include implementation |
| `STBTT_STATIC` | Make all functions static |
| `STBTT_RASTERIZER_VERSION` | 1 = old rasterizer, 2 = new (default) |
| `STBTT_malloc(x,u)` | Custom malloc |
| `STBTT_free(x,u)` | Custom free |
| `STBTT_assert(x)` | Custom assert |
| `STBTT_ifloor(x)` | Custom floor |
| `STBTT_iceil(x)` | Custom ceil |
| `STBTT_sqrt(x)` | Custom sqrt |
| `STBTT_fabs(x)` | Custom fabs |

### Key Types

```c
typedef struct {
    unsigned char *data;       // Pointer to font file data
    int fontstart;             // Offset of font in file
    int numGlyphs;             // Number of glyphs
    // ... internal fields ...
} stbtt_fontinfo;

typedef struct {
    unsigned short x0,y0,x1,y1;  // Bounding box in bitmap
    float xoff, yoff, xadvance;  // Positioning info
} stbtt_bakedchar;

typedef struct {
    float x0,y0,s0,t0;  // Top-left corner
    float x1,y1,s1,t1;  // Bottom-right corner
} stbtt_aligned_quad;
```

### Font Loading

```c
int stbtt_InitFont(stbtt_fontinfo *info,
                   const unsigned char *data, int offset);
```
Initialize font from memory buffer. Returns 0 on failure.

```c
int stbtt_GetFontOffsetForIndex(const unsigned char *data, int index);
```
Get font offset in TTC file. Returns -1 if index out of range.

```c
int stbtt_GetNumberOfFonts(const unsigned char *data);
```
Get number of fonts in file (usually 1, more for .ttc).

### Scaling

```c
float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels);
```
Get scale factor for font height in pixels.

```c
float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels);
```
Get scale factor for EM size in pixels (traditional point sizing).

### Glyph Metrics

```c
void stbtt_GetFontVMetrics(const stbtt_fontinfo *info,
                           int *ascent, int *descent, int *lineGap);
```
Get vertical font metrics (unscaled).

```c
void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int codepoint,
                                 int *advanceWidth, int *leftSideBearing);
```
Get horizontal metrics for a character (unscaled).

```c
int stbtt_GetCodepointKernAdvance(const stbtt_fontinfo *info, int ch1, int ch2);
```
Get kerning adjustment between two characters.

```c
int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int codepoint);
```
Convert unicode codepoint to glyph index (for efficiency).

### Simple Bitmap API (Quick Start)

```c
int stbtt_BakeFontBitmap(const unsigned char *data, int offset,
                         float pixel_height,
                         unsigned char *pixels, int pw, int ph,
                         int first_char, int num_chars,
                         stbtt_bakedchar *chardata);
```
Bake font to bitmap for texture. Returns first unused row.

```c
void stbtt_GetBakedQuad(const stbtt_bakedchar *chardata, int pw, int ph,
                        int char_index, float *xpos, float *ypos,
                        stbtt_aligned_quad *q, int opengl_fillrule);
```
Get quad coordinates for rendering a baked character.

### Advanced Packing API

```c
int stbtt_PackBegin(stbtt_pack_context *spc, unsigned char *pixels,
                    int width, int height, int stride_in_bytes,
                    int padding, void *alloc_context);
```
Initialize packing context for font atlas.

```c
void stbtt_PackSetOversampling(stbtt_pack_context *spc,
                                unsigned int h_oversample,
                                unsigned int v_oversample);
```
Set oversampling for improved quality at small sizes.

```c
int stbtt_PackFontRange(stbtt_pack_context *spc,
                        const unsigned char *fontdata, int font_index,
                        float font_size, int first_unicode_char_in_range,
                        int num_chars_in_range,
                        stbtt_packedchar *chardata_for_range);
```
Pack a range of characters into the atlas.

```c
void stbtt_PackEnd(stbtt_pack_context *spc);
```
Clean up packing context.

```c
void stbtt_GetPackedQuad(const stbtt_packedchar *chardata, int pw, int ph,
                         int char_index, float *xpos, float *ypos,
                         stbtt_aligned_quad *q, int align_to_integer);
```
Get quad for packed character.

### Direct Bitmap Rendering

```c
unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info,
                                         float scale_x, float scale_y,
                                         int codepoint,
                                         int *width, int *height,
                                         int *xoff, int *yoff);
```
Render a single character to an allocated bitmap.

```c
void stbtt_MakeCodepointBitmap(const stbtt_fontinfo *info,
                                unsigned char *output,
                                int out_w, int out_h, int out_stride,
                                float scale_x, float scale_y,
                                int codepoint);
```
Render into user-provided buffer.

```c
void stbtt_GetCodepointBitmapBox(const stbtt_fontinfo *info, int codepoint,
                                  float scale_x, float scale_y,
                                  int *ix0, int *iy0, int *ix1, int *iy1);
```
Get bitmap dimensions before rendering.

### SDF Rendering

```c
unsigned char *stbtt_GetCodepointSDF(const stbtt_fontinfo *info,
                                      float scale, int codepoint,
                                      int padding, unsigned char onedge_value,
                                      float pixel_dist_scale,
                                      int *width, int *height,
                                      int *xoff, int *yoff);
```
Generate Signed Distance Field bitmap for scalable fonts.

```c
void stbtt_FreeSDF(unsigned char *bitmap, void *userdata);
```
Free SDF bitmap.

### Shape Extraction

```c
int stbtt_GetCodepointShape(const stbtt_fontinfo *info, int codepoint,
                             stbtt_vertex **vertices);
```
Get glyph outline as vertices. Returns vertex count.

```c
void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *vertices);
```
Free shape data.

## Usage Examples

### Quick Start: Baked Font

```c
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

unsigned char ttf_buffer[1<<20];
unsigned char temp_bitmap[512*512];
stbtt_bakedchar cdata[96];

void init_font(void) {
    FILE *f = fopen("font.ttf", "rb");
    fread(ttf_buffer, 1, 1<<20, f);
    fclose(f);

    stbtt_BakeFontBitmap(ttf_buffer, 0, 32.0f,
                         temp_bitmap, 512, 512,
                         32, 96, cdata);
    // Upload temp_bitmap as texture
}

void draw_text(float x, float y, char *text) {
    while (*text) {
        if (*text >= 32 && *text < 128) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, 512, 512,
                              *text - 32, &x, &y, &q, 1);
            // Draw quad with texture coordinates
            // (q.x0,q.y0)-(q.x1,q.y1) with UV (q.s0,q.t0)-(q.s1,q.t1)
        }
        ++text;
    }
}
```

### Advanced: Packed Font Atlas

```c
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

stbtt_packedchar pdata[256];
unsigned char atlas[1024*1024];

void init_packed_font(const unsigned char *font_data) {
    stbtt_pack_context pc;

    stbtt_PackBegin(&pc, atlas, 1024, 1024, 0, 1, NULL);
    stbtt_PackSetOversampling(&pc, 2, 2);  // 2x oversampling
    stbtt_PackFontRange(&pc, font_data, 0, 24.0f, 0, 256, pdata);
    stbtt_PackEnd(&pc);
}
```

### Single Glyph Rendering

```c
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

void render_glyph(const unsigned char *font_data, int codepoint, float size) {
    stbtt_fontinfo font;
    stbtt_InitFont(&font, font_data, 0);

    float scale = stbtt_ScaleForPixelHeight(&font, size);

    int w, h, xoff, yoff;
    unsigned char *bitmap = stbtt_GetCodepointBitmap(&font, scale, scale,
                                                      codepoint, &w, &h,
                                                      &xoff, &yoff);

    // Use bitmap (w x h, 8-bit grayscale)

    stbtt_FreeBitmap(bitmap, NULL);
}
```

### Text Layout with Kerning

```c
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

void layout_text(stbtt_fontinfo *font, float scale, char *text) {
    float x = 0;
    int ascent, descent, lineGap;

    stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
    float baseline = ascent * scale;

    while (*text) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(font, *text, &advance, &lsb);

        // Render glyph at (x + lsb*scale, baseline)

        x += advance * scale;

        if (text[1]) {
            x += scale * stbtt_GetCodepointKernAdvance(font, text[0], text[1]);
        }
        ++text;
    }
}
```

### SDF Font for Scalable Text

```c
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

void generate_sdf_glyph(stbtt_fontinfo *font, int codepoint) {
    float scale = stbtt_ScaleForPixelHeight(font, 64);

    int w, h, xoff, yoff;
    unsigned char *sdf = stbtt_GetCodepointSDF(font, scale, codepoint,
                                                5,      // padding
                                                180,    // on-edge value
                                                36.0f,  // pixel_dist_scale
                                                &w, &h, &xoff, &yoff);

    // Use SDF bitmap - sample and compare to 180/255 for edge

    stbtt_FreeSDF(sdf, NULL);
}
```

## Key Concepts

### Coordinate System
- Y increases upward in font units
- Y increases downward in bitmap output
- Baseline is at y=0 in font coordinates

### Scaling
```c
// For pixel height:
scale = stbtt_ScaleForPixelHeight(&font, height_in_pixels);

// For point size (traditional):
scale = stbtt_ScaleForMappingEmToPixels(&font, point_size * dpi / 72.0f);
```

### Vertical Metrics
```
    ascent      (positive, above baseline)
   --------
   |      |
   | TEXT |    baseline at y=0
   |      |
   --------
    descent    (negative, below baseline)

    lineGap    (spacing to next line)
```

## VOS/TCC Compatibility Notes

### Compatible Features
- Font parsing works correctly
- Bitmap rendering is fully functional
- SDF generation works

### Requirements
- `<math.h>` for floor, ceil, sqrt, fabs, cos, acos, pow, fmod
- `<stdlib.h>` for malloc, free
- `<assert.h>` for assert
- `<string.h>` for strlen, memcpy, memset

### Recommended Configuration for VOS

```c
// For VOS/TCC compatibility
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
```

### Custom Allocator for VOS

```c
void *vos_malloc(size_t size, void *user);
void vos_free(void *ptr, void *user);

#define STBTT_malloc(x,u) vos_malloc(x,u)
#define STBTT_free(x,u)   vos_free(x,u)

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
```

### Memory Considerations
- Font file must stay in memory while font is in use
- Bitmaps are allocated with malloc (or custom allocator)
- Free bitmaps with `stbtt_FreeBitmap()` or `stbtt_FreeSDF()`
- Pack context uses temporary allocations

### Performance Notes
- Converting codepoint to glyph index for each operation is slow
- Use `stbtt_FindGlyphIndex()` once and glyph-based functions after
- Cache rendered glyphs in texture atlases
- SDF generation is slow but produces scalable results
