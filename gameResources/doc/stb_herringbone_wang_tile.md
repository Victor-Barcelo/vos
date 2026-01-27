# stb_herringbone_wang_tile.h

Herringbone Wang Tile Generator for procedural texture/map generation.

## Source

- **Repository**: [https://github.com/nothings/stb](https://github.com/nothings/stb)
- **Direct Link**: [https://github.com/nothings/stb/blob/master/stb_herringbone_wang_tile.h](https://github.com/nothings/stb/blob/master/stb_herringbone_wang_tile.h)
- **Additional Resources**: [https://nothings.org/gamedev/herringbone/](https://nothings.org/gamedev/herringbone/)
- **Version**: 0.7
- **Author**: Sean Barrett

## License

Public domain / MIT dual license. You can choose whichever you prefer.

## Description

stb_herringbone_wang_tile is an SDK for Herringbone Wang Tile generation. Herringbone Wang Tiling is a technique for "randomly" tiling the plane with a set of tiles that can produce seamless, non-repeating patterns. This is useful for procedurally generating textures, terrain, or other tile-based content.

The library workflow:
1. Use the library offline to generate a "template" image
2. Edit the template tiles by hand in an image editor
3. Load the edited tile image back into the library
4. Generate maps at runtime using the tileset

## Features

- Template generation for creating new tilesets
- Runtime map generation from tilesets
- Corner-based or edge-based color matching
- Configurable number of colors per constraint type
- Repetition reduction to avoid obvious patterns
- Supports variable tile variations
- Memory efficient (no allocation during map generation after tileset load)

## How It Works

### Wang Tiles
Wang tiles are square tiles with colored edges. Tiles can only be placed adjacent to tiles with matching edge colors, ensuring seamless transitions.

### Herringbone Pattern
Instead of a regular grid, tiles are arranged in a herringbone pattern using two tile orientations:
- Horizontal tiles: 2n x n pixels
- Vertical tiles: n x 2n pixels

This creates more visually interesting and less obviously repetitive results than standard square tilings.

### Constraint Types

**Corner-based (4 corner types):**
```
    0---*---1---*---2---*---3
    |       |               |
    *       *               *
    |       |               |
1---*---2---*---3       0---*---1---*---2
|               |       |
*               *       *
|               |       |
0---*---1---*---2---*---3
```

**Edge-based (6 edge types):**
```
*---2---*---3---*      *---0---*
|               |      |       |
1               4      5       1
|               |      |       |
*---0---*---2---*      *       *
                       |       |
                       4       5
                       |       |
                       *---3---*
```

## Configuration Macros

```c
#define STB_HERRINGBONE_WANG_TILE_IMPLEMENTATION  // Required for implementation

// Optional: Override random number generator
#define STB_HBWANG_RAND()  (rand() >> 4)

// Optional: Override assert
#define STB_HBWANG_ASSERT(x)  assert(x)

// Optional: Make all symbols static
#define STB_HBWANG_STATIC

// Optional: Disable repetition reduction
#define STB_HBWANG_NO_REPITITION_REDUCTION

// Optional: Set max generated map size (in tile short-sides)
#define STB_HBWANG_MAX_X  100  // Default
#define STB_HBWANG_MAX_Y  100  // Default
```

## API Reference

### Types

```c
typedef struct stbhw_tileset stbhw_tileset;
typedef struct stbhw_tile stbhw_tile;
typedef struct stbhw_config stbhw_config;

struct stbhw_tileset {
   int is_corner;          // Using corner or edge colors
   int num_color[6];       // Number of colors per type
   int short_side_len;     // Base tile dimension
   stbhw_tile **h_tiles;   // Horizontal tiles
   stbhw_tile **v_tiles;   // Vertical tiles
   int num_h_tiles, max_h_tiles;
   int num_v_tiles, max_v_tiles;
};

struct stbhw_config {
   int is_corner;          // 1 for corner colors, 0 for edge colors
   int short_side_len;     // Tile dimension (tiles are 2n x n or n x 2n)
   int num_color[6];       // Colors per constraint (4 if corner, 6 if edge)
                          // Legal values: 1-8 for edge, 1-4 for corner
   int num_vary_x;         // Additional X variations in template
   int num_vary_y;         // Additional Y variations in template
   int corner_type_color_template[4][4];  // Corner markup options
};

struct stbhw_tile {
   signed char a, b, c, d, e, f;  // Edge/corner constraints
   unsigned char pixels[1];       // Tile pixel data (RGB, row-major)
};
```

### Error Handling

```c
const char *stbhw_get_last_error(void);
// Returns description of last error (not thread-safe)
// Returns NULL if no error
```

### Template Generation

```c
void stbhw_get_template_size(stbhw_config *c, int *w, int *h);
// Computes the size needed for the template image

int stbhw_make_template(stbhw_config *c, unsigned char *data,
                        int w, int h, int stride_in_bytes);
// Generates a template image
// data: buffer for 3*w*h bytes (RGB format)
// Returns non-zero on success, 0 on error
```

### Tileset Loading

```c
int stbhw_build_tileset_from_image(stbhw_tileset *ts,
                                   unsigned char *pixels,
                                   int stride_in_bytes,
                                   int w, int h);
// Build a tileset from a template-based image
// You allocate stbhw_tileset, function fills it out
// Individual tiles are malloc'd internally
// Returns non-zero on success, 0 on error

void stbhw_free_tileset(stbhw_tileset *ts);
// Free a tileset built by stbhw_build_tileset_from_image
```

### Map Generation

```c
int stbhw_generate_image(stbhw_tileset *ts, int **weighting,
                         unsigned char *pixels,
                         int stride_in_bytes,
                         int w, int h);
// Generate a map image using the tileset
// weighting: should be NULL (non-NULL untested)
// pixels: output buffer (3 bytes per pixel, RGB)
// Not thread-safe (uses global data)
// Returns non-zero on success, 0 on error
```

## Usage Example

### Creating a Tileset Template

```c
#define STB_HERRINGBONE_WANG_TILE_IMPLEMENTATION
#include "stb_herringbone_wang_tile.h"

void create_template(void) {
    stbhw_config config = {0};

    config.is_corner = 1;           // Use corner constraints
    config.short_side_len = 32;     // 32x64 or 64x32 tiles
    config.num_color[0] = 2;        // 2 colors for corner type 0
    config.num_color[1] = 2;
    config.num_color[2] = 2;
    config.num_color[3] = 2;
    config.num_vary_x = 1;          // Variations
    config.num_vary_y = 1;

    int w, h;
    stbhw_get_template_size(&config, &w, &h);

    unsigned char *data = malloc(3 * w * h);

    if (stbhw_make_template(&config, data, w, h, w * 3)) {
        // Save template to file
        stbi_write_png("template.png", w, h, 3, data, w * 3);
    }

    free(data);
}
```

### Loading and Using a Tileset

```c
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_HERRINGBONE_WANG_TILE_IMPLEMENTATION
#include "stb_herringbone_wang_tile.h"

int main(int argc, char **argv) {
    unsigned char *data;
    int w, h;
    stbhw_tileset ts;

    // Load the edited tileset image
    data = stbi_load("mytiles.png", &w, &h, NULL, 3);
    if (data == NULL) {
        fprintf(stderr, "Error loading tileset\n");
        return 1;
    }

    // Build tileset from image
    if (!stbhw_build_tileset_from_image(&ts, data, w * 3, w, h)) {
        fprintf(stderr, "Error: %s\n", stbhw_get_last_error());
        return 1;
    }
    free(data);

    // Generate a map
    int map_w = 512, map_h = 512;
    data = malloc(3 * map_w * map_h);

    srand(time(NULL));
    if (!stbhw_generate_image(&ts, NULL, data, map_w * 3, map_w, map_h)) {
        fprintf(stderr, "Error: %s\n", stbhw_get_last_error());
        return 1;
    }

    // Save generated map
    stbi_write_png("generated_map.png", map_w, map_h, 3, data, map_w * 3);

    stbhw_free_tileset(&ts);
    free(data);

    return 0;
}
```

## Tile Constraint Diagrams

### Horizontal Tile (2n x n)

**Corner constraints:**
```
a-----b-----c
|           |
|           |
|           |
d-----e-----f
```

**Edge constraints:**
```
*---a---*---b---*
|               |
c               d
|               |
*---e---*---f---*
```

### Vertical Tile (n x 2n)

**Corner constraints:**
```
a-----d
|     |
|     |
|     |
b     e
|     |
|     |
|     |
c-----f
```

**Edge constraints:**
```
*---a---*
|       |
b       c
|       |
*       *
|       |
d       e
|       |
*---f---*
```

## Sample Tilesets

Sample tilesets are available at:
- [https://nothings.org/gamedev/herringbone/](https://nothings.org/gamedev/herringbone/)
- GitHub repository `data/herringbone` directory

## VOS/TCC Compatibility Notes

### Compatibility Status

This library should work well with TCC and VOS:

1. **Simple Dependencies**: Only requires `stdlib.h`, `string.h`, and optionally `assert.h`

2. **No Complex Features**: Uses basic C constructs

3. **Configurable Memory**: No runtime allocation during map generation

### Potential Issues

1. **Global Variables**: Uses static arrays for color assignments (`c_color`, `v_color`, `h_color`). Size depends on `STB_HBWANG_MAX_X/Y`.

2. **malloc Usage**: Tileset loading uses `malloc()` for tile data

3. **Floating-Point**: No floating-point operations in core algorithms

### Recommended Configuration for VOS

```c
// Reduce max map size for smaller memory footprint
#define STB_HBWANG_MAX_X  50   // Smaller max width
#define STB_HBWANG_MAX_Y  50   // Smaller max height

// Use VOS random if available
#define STB_HBWANG_RAND()  vos_random()

#define STB_HERRINGBONE_WANG_TILE_IMPLEMENTATION
#include "stb_herringbone_wang_tile.h"
```

### Memory Usage

Global arrays:
- `c_color`: (MAX_Y+6) * (MAX_X+6) bytes
- `v_color`: (MAX_Y+6) * (MAX_X+5) bytes
- `h_color`: (MAX_Y+5) * (MAX_X+6) bytes

For default 100x100: ~31KB
For 50x50: ~8KB

Tileset memory depends on tile count and size.

### Integration Example for VOS

```c
// vos_terrain.c - Procedural terrain generation
#define STB_HBWANG_MAX_X  32
#define STB_HBWANG_MAX_Y  32
#define STB_HBWANG_RAND()  (vos_rand() >> 4)
#define STB_HERRINGBONE_WANG_TILE_IMPLEMENTATION
#include "stb_herringbone_wang_tile.h"

static stbhw_tileset terrain_tiles;
static int tileset_loaded = 0;

int load_terrain_tileset(unsigned char *image_data, int w, int h) {
    if (stbhw_build_tileset_from_image(&terrain_tiles, image_data, w*3, w, h)) {
        tileset_loaded = 1;
        return 1;
    }
    return 0;
}

int generate_terrain(unsigned char *output, int w, int h) {
    if (!tileset_loaded) return 0;
    return stbhw_generate_image(&terrain_tiles, NULL, output, w*3, w, h);
}

void cleanup_terrain(void) {
    if (tileset_loaded) {
        stbhw_free_tileset(&terrain_tiles);
        tileset_loaded = 0;
    }
}
```

### Use Cases for VOS

1. **Procedural Terrain**: Generate varied terrain textures for backgrounds
2. **Dungeon Generation**: Create seamless dungeon floor/wall patterns
3. **Texture Synthesis**: Produce non-repeating textures from small samples
4. **Level Decoration**: Add visual variety to tile-based levels
