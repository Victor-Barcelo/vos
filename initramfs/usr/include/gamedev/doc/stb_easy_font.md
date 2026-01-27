# stb_easy_font.h - Simple Bitmap Font

## Overview

**stb_easy_font.h** is a single-header library for quick-and-dirty text rendering in 3D applications. It generates vertex data for text using a built-in bitmap font, perfect for debug output, frame rate display, or any situation where you need text fast without setting up proper font rendering.

## Source

- **Original Repository:** https://github.com/nothings/stb
- **File:** stb_easy_font.h
- **Version:** 1.1
- **Author:** Sean Barrett (Feb 2015)

## License

This software is dual-licensed:
- **Public Domain** (www.unlicense.org)
- **MIT License**

Choose whichever you prefer.

## Features

- No texture loading required
- Generates quad vertices directly
- ASCII characters only (32-126)
- Multiline text support (via '\n')
- Adjustable character spacing
- Returns dimensions for layout
- Single header, no dependencies beyond math.h
- All functions are static (no linker conflicts)

## Limitations

- ASCII only (no Unicode)
- Fixed, low-resolution font
- Not performance-optimized
- Visual quality is intentionally minimal
- Quads-based (may need conversion for modern APIs)

## API Reference

### Functions

```c
int stb_easy_font_print(float x, float y, char *text,
                        unsigned char color[4],
                        void *vertex_buffer, int vbuf_size);
```

Generates vertex data for rendering text.

**Parameters:**
- `x, y` - Starting position (top-left of first character)
- `text` - String to render (can contain '\n')
- `color` - RGBA color (NULL = white 255,255,255,255)
- `vertex_buffer` - Buffer to fill with vertex data
- `vbuf_size` - Size of buffer in bytes

**Returns:** Number of quads generated

**Vertex Format:**
```
x: float      (offset 0)
y: float      (offset 4)
z: float      (offset 8)
color: uint8[4] (offset 12)
Total: 16 bytes per vertex, 64 bytes per quad
```

**Notes:**
- Expect ~270 bytes per character on average
- Truncates if buffer is too small
- X increases rightward, Y increases downward

```c
int stb_easy_font_width(char *text);
```

Returns the horizontal extent of the text in pixels.
- Handles multiline text (returns maximum line width)

```c
int stb_easy_font_height(char *text);
```

Returns the vertical extent of the text in pixels.
- Each line is 12 pixels tall

```c
void stb_easy_font_spacing(float spacing);
```

Adjusts character spacing.

**Parameters:**
- `spacing` - Pixels to add between characters
  - Positive values expand spacing
  - Negative values contract (minimum -1.5)
  - 0 = default spacing

## Usage Examples

### OpenGL (Legacy/Immediate Mode)

```c
#include "stb_easy_font.h"

void print_string(float x, float y, char *text, float r, float g, float b) {
    static char buffer[99999];  // ~500 chars max
    int num_quads;

    num_quads = stb_easy_font_print(x, y, text, NULL, buffer, sizeof(buffer));

    glColor3f(r, g, b);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buffer);
    glDrawArrays(GL_QUADS, 0, num_quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}
```

### Custom Color

```c
#include "stb_easy_font.h"

void print_colored_text(float x, float y, char *text,
                        unsigned char r, unsigned char g,
                        unsigned char b, unsigned char a) {
    static char buffer[99999];
    unsigned char color[4] = {r, g, b, a};

    int num_quads = stb_easy_font_print(x, y, text, color, buffer, sizeof(buffer));

    // Render num_quads * 4 vertices...
}
```

### Text Dimensions

```c
#include "stb_easy_font.h"

void center_text(float screen_width, float screen_height, char *text) {
    int text_width = stb_easy_font_width(text);
    int text_height = stb_easy_font_height(text);

    float x = (screen_width - text_width) / 2.0f;
    float y = (screen_height - text_height) / 2.0f;

    // Render text at (x, y)
}
```

### Multiline Text

```c
#include "stb_easy_font.h"

void print_multiline(void) {
    char *text = "Line 1\nLine 2\nLine 3";
    static char buffer[99999];

    int height = stb_easy_font_height(text);  // Returns 36 (3 * 12)
    int width = stb_easy_font_width(text);    // Returns max line width

    int num_quads = stb_easy_font_print(10, 10, text, NULL, buffer, sizeof(buffer));
}
```

### Modern OpenGL / Converting to Triangles

```c
#include "stb_easy_font.h"

// For APIs that don't support quads, convert to triangles
void setup_index_buffer(unsigned short *indices, int max_quads) {
    for (int i = 0; i < max_quads; i++) {
        // Two triangles per quad
        indices[i * 6 + 0] = i * 4 + 0;
        indices[i * 6 + 1] = i * 4 + 1;
        indices[i * 6 + 2] = i * 4 + 2;
        indices[i * 6 + 3] = i * 4 + 0;
        indices[i * 6 + 4] = i * 4 + 2;
        indices[i * 6 + 5] = i * 4 + 3;
    }
}

void render_text_triangles(float x, float y, char *text) {
    static char buffer[99999];
    static unsigned short indices[6000];  // For ~1000 quads
    static int indices_initialized = 0;

    if (!indices_initialized) {
        setup_index_buffer(indices, 1000);
        indices_initialized = 1;
    }

    int num_quads = stb_easy_font_print(x, y, text, NULL, buffer, sizeof(buffer));

    // Render using indexed triangles
    // glDrawElements(GL_TRIANGLES, num_quads * 6, GL_UNSIGNED_SHORT, indices);
}
```

### Framebuffer-Based Rendering

```c
#include "stb_easy_font.h"
#include <string.h>

// Render to a software framebuffer
void render_text_to_buffer(unsigned int *framebuffer, int fb_width, int fb_height,
                           float x, float y, char *text, unsigned int color) {
    char vertex_buffer[99999];
    int num_quads = stb_easy_font_print(x, y, text, NULL, vertex_buffer, sizeof(vertex_buffer));

    // Extract quads and rasterize
    float *verts = (float *)vertex_buffer;
    for (int q = 0; q < num_quads; q++) {
        // Get quad corners (4 vertices * 4 floats per vertex)
        int base = q * 16;
        float x0 = verts[base + 0], y0 = verts[base + 1];
        float x1 = verts[base + 4], y1 = verts[base + 5];
        float x2 = verts[base + 8], y2 = verts[base + 9];

        // Simple filled rectangle (quads are axis-aligned)
        int ix0 = (int)x0, iy0 = (int)y0;
        int ix1 = (int)x2, iy1 = (int)y2;

        for (int py = iy0; py < iy1 && py < fb_height; py++) {
            if (py < 0) continue;
            for (int px = ix0; px < ix1 && px < fb_width; px++) {
                if (px < 0) continue;
                framebuffer[py * fb_width + px] = color;
            }
        }
    }
}
```

### Adjusting Spacing

```c
#include "stb_easy_font.h"

void print_compact_text(float x, float y, char *text) {
    static char buffer[99999];

    // Tighter spacing
    stb_easy_font_spacing(-0.5f);

    int num_quads = stb_easy_font_print(x, y, text, NULL, buffer, sizeof(buffer));

    // Reset to default
    stb_easy_font_spacing(0);
}
```

## VOS/TCC Compatibility Notes

### Compatible Features
- All functions work correctly with TCC
- No dynamic memory allocation
- Simple float arithmetic only

### Requirements
- Include `<stdlib.h>` (for NULL)
- Include `<math.h>` (for ceil)

### Recommended Configuration for VOS

```c
// For VOS/TCC compatibility
#include <stdlib.h>
#include <math.h>
#include "stb_easy_font.h"
```

### Memory Considerations
- Pre-allocate sufficient buffer space
- ~270 bytes per character is typical
- Stack allocation works well for short strings
- For long text, use heap allocation or static buffers

### Rendering Without OpenGL

For VOS environments without OpenGL, use the framebuffer approach shown above, or extract the quad vertices and render them with your custom graphics system.

### Vertex Data Structure

```c
// Each quad consists of 4 vertices, each vertex is:
typedef struct {
    float x, y, z;
    unsigned char color[4];
} EasyFontVertex;

// Total: 16 bytes per vertex, 64 bytes per quad
// Vertices are ordered: top-left, top-right, bottom-right, bottom-left
```

### Performance Notes
- The library is intentionally simple, not optimized
- For high-performance text, use stb_truetype with texture caching
- Best used for occasional debug output, not continuous text rendering
