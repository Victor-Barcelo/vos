# stb_tilemap_editor.h

Embeddable tilemap editor for C/C++.

## Source

- **Repository**: [https://github.com/nothings/stb](https://github.com/nothings/stb)
- **Direct Link**: [https://github.com/nothings/stb/blob/master/stb_tilemap_editor.h](https://github.com/nothings/stb/blob/master/stb_tilemap_editor.h)
- **Version**: 0.42
- **Author**: Sean Barrett

## License

Public domain / MIT dual license. You can choose whichever you prefer.

## Description

stb_tilemap_editor is a single-header embeddable tilemap editor that can be integrated directly into games and applications. It allows in-game editing of tilemaps without needing to switch to external tools.

A tilemap in this library is defined as an array of rectangles, where each rectangle contains a small stack of images (up to 32 layers).

## Features

- Embeddable editor that runs inside your application
- Support for multiple map layers (up to 32 layers)
- Tile stacking on each map cell (up to 32 images per cell)
- Category-based tile organization in the palette
- Undo/redo support with configurable buffer size
- Copy/cut/paste support across maps
- Properties panel for tile-based object editing
- Tile linking for connecting map elements (e.g., doors to switches)
- Keyboard shortcuts and mouse-based editing
- SDL mouse event integration support
- Customizable display scaling and spacing

## Limitations

- Maps are limited to 4096x4096 in dimension
- Each map square can only contain a stack of at most 32 images
- A map can only use up to 32768 distinct image tiles
- Only one active editor instance at a time
- No built-in file format (you implement save/load)

## Configuration Macros

Define these before including the header:

```c
#define STBTE_MAX_TILEMAP_X      200   // max 4096
#define STBTE_MAX_TILEMAP_Y      200   // max 4096
#define STBTE_MAX_LAYERS         8     // max 32
#define STBTE_MAX_CATEGORIES     100
#define STBTE_UNDO_BUFFER_BYTES  (1 << 24) // 16 MB
#define STBTE_MAX_COPY           90000  // e.g. 300x300
#define STBTE_MAX_PROPERTIES     10     // max properties per tile
```

## Required Callback Macros

You must define these before including with `STB_TILEMAP_EDITOR_IMPLEMENTATION`:

```c
void STBTE_DRAW_RECT(int x0, int y0, int x1, int y1, unsigned int color);
// Draw a filled rectangle (exclusive on right/bottom)
// color = (r<<16)|(g<<8)|(b)

void STBTE_DRAW_TILE(int x0, int y0, unsigned short id, int highlight, float *data);
// Draw the tile image identified by 'id'
// highlight: STBTE_drawmode_deemphasize, STBTE_drawmode_normal, or STBTE_drawmode_emphasize
// data: NULL for palette display, otherwise per-tile property data
```

## API Reference

### Types

```c
typedef struct stbte_tilemap stbte_tilemap;

// Draw modes for STBTE_DRAW_TILE callback
enum {
   STBTE_drawmode_deemphasize = -1,
   STBTE_drawmode_normal      =  0,
   STBTE_drawmode_emphasize   =  1,
};

// Property types
#define STBTE_PROP_none     0
#define STBTE_PROP_int      1
#define STBTE_PROP_float    2
#define STBTE_PROP_bool     3
#define STBTE_PROP_disabled 4

// Special tile value
#define STBTE_EMPTY    -1
```

### Creation Functions

```c
stbte_tilemap *stbte_create_map(int map_x, int map_y, int map_layers,
                                 int spacing_x, int spacing_y, int max_tiles);
// Create an editable tilemap
// map_x, map_y: dimensions of map (user can change in editor)
// map_layers: number of layers (fixed)
// spacing_x, spacing_y: initial distance between tile edges in editor pixels
// max_tiles: maximum number of distinct tiles that can be defined
// Returns NULL on insufficient memory

void stbte_define_tile(stbte_tilemap *tm, unsigned short id,
                       unsigned int layermask, const char *category);
// Define a tile for use in the editor
// id: unique identifier, 0 <= id < 32768
// layermask: bitmask of allowed layers (1 = layer 0, 255 = layers 0..7)
// category: category name for grouping in palette
```

### Display Functions

```c
void stbte_set_display(int x0, int y0, int x1, int y1);
// Set the editor display area; call again on resize

void stbte_set_spacing(stbte_tilemap *tm, int spacing_x, int spacing_y,
                       int palette_spacing_x, int palette_spacing_y);
// Set map and palette tile spacing (for zooming)

void stbte_set_sidewidths(int left, int right);
// Set left and right side panel widths
```

### Frame Update Functions

```c
void stbte_draw(stbte_tilemap *tm);
// Call each frame to render the editor

void stbte_tick(stbte_tilemap *tm, float time_in_seconds_since_last_frame);
// Call each frame for animations/updates
```

### Input Functions

```c
void stbte_mouse_sdl(stbte_tilemap *tm, const void *sdl_event,
                     float xscale, float yscale, int xoffset, int yoffset);
// Process SDL mouse events (MOUSEMOTION, MOUSEBUTTONDOWN, MOUSEBUTTONUP, MOUSEWHEEL)

void stbte_mouse_move(stbte_tilemap *tm, int x, int y, int shifted, int scrollkey);
void stbte_mouse_button(stbte_tilemap *tm, int x, int y, int right, int down,
                        int shifted, int scrollkey);
void stbte_mouse_wheel(stbte_tilemap *tm, int x, int y, int vscroll);
// Direct mouse input functions

enum stbte_action {
   STBTE_tool_select,
   STBTE_tool_brush,
   STBTE_tool_erase,
   STBTE_tool_rectangle,
   STBTE_tool_eyedropper,
   STBTE_tool_link,
   STBTE_act_toggle_grid,
   STBTE_act_toggle_links,
   STBTE_act_undo,
   STBTE_act_redo,
   STBTE_act_cut,
   STBTE_act_copy,
   STBTE_act_paste,
   STBTE_scroll_left,
   STBTE_scroll_right,
   STBTE_scroll_up,
   STBTE_scroll_down,
};
void stbte_action(stbte_tilemap *tm, enum stbte_action act);
// Trigger keyboard actions
```

### Save/Load Functions

```c
void stbte_get_dimensions(stbte_tilemap *tm, int *max_x, int *max_y);
// Get current map dimensions

short* stbte_get_tile(stbte_tilemap *tm, int x, int y);
// Get tile stack at position (array of map_layers shorts, STBTE_EMPTY for empty)

float *stbte_get_properties(stbte_tilemap *tm, int x, int y);
// Get property array for tile at x,y (STBTE_MAX_PROPERTIES floats)

void stbte_get_link(stbte_tilemap *tm, int x, int y, int *destx, int *desty);
// Get link destination for tile at x,y

void stbte_set_dimensions(stbte_tilemap *tm, int max_x, int max_y);
// Set map dimensions

void stbte_clear_map(stbte_tilemap *tm);
// Clear the entire map

void stbte_set_tile(stbte_tilemap *tm, int x, int y, int layer, signed short tile);
// Set a tile (use STBTE_EMPTY to clear)

void stbte_set_property(stbte_tilemap *tm, int x, int y, int n, float val);
// Set property n for tile at x,y

void stbte_set_link(stbte_tilemap *tm, int x, int y, int destx, int desty);
// Set link from x,y to destx,desty (-1,-1 for no link)
```

### Optional Functions

```c
void stbte_set_background_tile(stbte_tilemap *tm, short id);
// Set the tile to fill the bottom layer with

void stbte_set_layername(stbte_tilemap *tm, int layer, const char *layername);
// Set display name for a layer (0..map_layers-1)
```

## Usage Example

```c
#define STB_TILEMAP_EDITOR_IMPLEMENTATION

// Required callbacks
void STBTE_DRAW_RECT(int x0, int y0, int x1, int y1, unsigned int color) {
    // Draw filled rectangle using your graphics API
    int r = (color >> 16) & 0xFF;
    int g = (color >> 8) & 0xFF;
    int b = color & 0xFF;
    draw_filled_rect(x0, y0, x1 - x0, y1 - y0, r, g, b);
}

void STBTE_DRAW_TILE(int x0, int y0, unsigned short id, int highlight, float *data) {
    // Draw tile 'id' at position x0,y0
    // Apply highlight effect based on drawmode
    draw_tile_image(tile_textures[id], x0, y0);
}

#include "stb_tilemap_editor.h"

// Create and initialize the editor
stbte_tilemap *tm = stbte_create_map(100, 100, 4, 32, 32, 256);

// Define tiles
stbte_define_tile(tm, 0, 0x01, "Ground");   // Layer 0 only
stbte_define_tile(tm, 1, 0x01, "Ground");
stbte_define_tile(tm, 2, 0x02, "Objects");  // Layer 1 only
stbte_define_tile(tm, 3, 0x0F, "Special");  // Layers 0-3

// Set display area
stbte_set_display(0, 0, 800, 600);

// Main loop
while (running) {
    // Process input
    stbte_mouse_sdl(tm, &event, 1.0f, 1.0f, 0, 0);

    // Update
    stbte_tick(tm, delta_time);

    // Render
    stbte_draw(tm);
}

// Save map data
int mx, my;
stbte_get_dimensions(tm, &mx, &my);
for (int y = 0; y < my; y++) {
    for (int x = 0; x < mx; x++) {
        short *tiles = stbte_get_tile(tm, x, y);
        // Save tiles[0..map_layers-1]
    }
}
```

## VOS/TCC Compatibility Notes

### Potential Issues

1. **Large Memory Allocation**: The editor allocates all memory upfront based on configuration macros. Reduce `STBTE_MAX_TILEMAP_X/Y` and `STBTE_UNDO_BUFFER_BYTES` for memory-constrained environments.

2. **Floating-Point Math**: Property editing uses floating-point operations. Ensure TCC floating-point support is enabled.

3. **Static Variables**: The library uses static variables for undo buffer and clipboard, which may need adjustment for restricted environments.

4. **Callback Requirements**: You must implement drawing callbacks that interface with your graphics system.

### Recommended Configuration for VOS

```c
#define STBTE_MAX_TILEMAP_X      64    // Reduced for memory
#define STBTE_MAX_TILEMAP_Y      64
#define STBTE_MAX_LAYERS         4
#define STBTE_MAX_CATEGORIES     16
#define STBTE_UNDO_BUFFER_BYTES  (1 << 16)  // 64KB
#define STBTE_MAX_COPY           4096

void STBTE_DRAW_RECT(int x0, int y0, int x1, int y1, unsigned int color) {
    // Use VOS framebuffer drawing
}

void STBTE_DRAW_TILE(int x0, int y0, unsigned short id, int highlight, float *data) {
    // Use VOS sprite/tile rendering
}

#define STB_TILEMAP_EDITOR_IMPLEMENTATION
#include "stb_tilemap_editor.h"
```

### Integration Tips

- Use the editor for development-time map creation, then export to a simpler runtime format
- The editor can be conditionally compiled out of release builds
- Consider implementing a custom file format that's efficient for your target platform
