# stb_rect_pack.h - Rectangle Packing

## Overview

**stb_rect_pack.h** is a single-header library for packing rectangular areas into a larger rectangle (bin packing). It's particularly useful for creating texture atlases, sprite sheets, and UI layout optimization.

## Source

- **Original Repository:** https://github.com/nothings/stb
- **File:** stb_rect_pack.h
- **Version:** 1.01
- **Author:** Sean Barrett (2014)

## License

This software is dual-licensed:
- **Public Domain** (www.unlicense.org)
- **MIT License**

Choose whichever you prefer.

## Features

- Skyline Bottom-Left algorithm for efficient packing
- Two heuristic options for different use cases
- No dynamic memory allocation (user provides node buffer)
- Returns packing success/failure result
- Handles empty rectangles (w=0 or h=0)
- Gracefully fails for too-wide rectangles
- Multiple packing passes supported
- Designed to integrate with stb_truetype.h for font atlas generation

## API Reference

### Compile-Time Options

| Macro | Description |
|-------|-------------|
| `STB_RECT_PACK_IMPLEMENTATION` | Define in ONE file to include implementation |
| `STBRP_STATIC` | Make all functions static |
| `STBRP_SORT` | Override qsort function |
| `STBRP_ASSERT` | Override assert function |

### Types

```c
typedef int stbrp_coord;  // Coordinate type

struct stbrp_rect {
    int         id;          // User data (for your use)
    stbrp_coord w, h;        // Input: rectangle dimensions
    stbrp_coord x, y;        // Output: packed position
    int         was_packed;  // Output: non-zero if packed successfully
};

struct stbrp_node {
    stbrp_coord  x, y;
    stbrp_node  *next;
};

struct stbrp_context {
    int width;
    int height;
    int align;
    int init_mode;
    int heuristic;
    int num_nodes;
    stbrp_node *active_head;
    stbrp_node *free_head;
    stbrp_node extra[2];
};
```

### Constants

```c
#define STBRP__MAXVAL  0x7fffffff  // Maximum supported coordinate value

enum {
    STBRP_HEURISTIC_Skyline_default = 0,
    STBRP_HEURISTIC_Skyline_BL_sortHeight = STBRP_HEURISTIC_Skyline_default,
    STBRP_HEURISTIC_Skyline_BF_sortHeight
};
```

### Functions

```c
void stbrp_init_target(stbrp_context *context,
                       int width, int height,
                       stbrp_node *nodes, int num_nodes);
```

Initialize the packing context for a target rectangle.

**Parameters:**
- `context` - Packing context to initialize
- `width, height` - Dimensions of target area
- `nodes` - Temporary node storage (user-allocated)
- `num_nodes` - Number of nodes provided

**Notes:**
- For best results: `num_nodes >= width`
- Alternatively, call `stbrp_setup_allow_out_of_mem(context, 1)`

```c
int stbrp_pack_rects(stbrp_context *context,
                     stbrp_rect *rects, int num_rects);
```

Pack rectangles into the target area.

**Parameters:**
- `context` - Initialized packing context
- `rects` - Array of rectangles to pack
- `num_rects` - Number of rectangles

**Returns:**
- 1 if all rectangles were packed successfully
- 0 if some rectangles did not fit

**Output:**
- Sets `x`, `y`, and `was_packed` for each rectangle

```c
void stbrp_setup_allow_out_of_mem(stbrp_context *context, int allow_out_of_mem);
```

Configure out-of-memory handling.

**Parameters:**
- `allow_out_of_mem` - If 1, better packing but may fail; if 0, quantizes widths to guarantee success

```c
void stbrp_setup_heuristic(stbrp_context *context, int heuristic);
```

Select packing heuristic.

**Parameters:**
- `heuristic` - One of `STBRP_HEURISTIC_Skyline_BL_sortHeight` or `STBRP_HEURISTIC_Skyline_BF_sortHeight`

## Usage Examples

### Basic Rectangle Packing

```c
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

int pack_textures(void) {
    int atlas_width = 512;
    int atlas_height = 512;

    // Allocate nodes (at least width for best results)
    stbrp_node nodes[512];
    stbrp_context context;

    // Initialize packer
    stbrp_init_target(&context, atlas_width, atlas_height, nodes, 512);

    // Define rectangles to pack
    stbrp_rect rects[3];
    rects[0].id = 0; rects[0].w = 100; rects[0].h = 50;
    rects[1].id = 1; rects[1].w = 64;  rects[1].h = 64;
    rects[2].id = 2; rects[2].w = 128; rects[2].h = 32;

    // Pack rectangles
    int all_packed = stbrp_pack_rects(&context, rects, 3);

    if (all_packed) {
        for (int i = 0; i < 3; i++) {
            printf("Rect %d: (%d, %d)\n",
                   rects[i].id, rects[i].x, rects[i].y);
        }
    } else {
        printf("Some rectangles did not fit!\n");
        for (int i = 0; i < 3; i++) {
            if (!rects[i].was_packed) {
                printf("Rect %d failed to pack\n", rects[i].id);
            }
        }
    }

    return all_packed;
}
```

### Texture Atlas Generation

```c
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

typedef struct {
    int id;
    int width, height;
    unsigned char *pixels;
} Sprite;

int create_texture_atlas(Sprite *sprites, int num_sprites,
                         unsigned char *atlas, int atlas_w, int atlas_h) {
    // Prepare packing nodes
    stbrp_node *nodes = malloc(sizeof(stbrp_node) * atlas_w);
    stbrp_context context;

    stbrp_init_target(&context, atlas_w, atlas_h, nodes, atlas_w);

    // Allow out of mem for better packing
    stbrp_setup_allow_out_of_mem(&context, 1);

    // Prepare rectangles
    stbrp_rect *rects = malloc(sizeof(stbrp_rect) * num_sprites);
    for (int i = 0; i < num_sprites; i++) {
        rects[i].id = sprites[i].id;
        rects[i].w = sprites[i].width;
        rects[i].h = sprites[i].height;
    }

    // Pack
    int success = stbrp_pack_rects(&context, rects, num_sprites);

    if (success) {
        // Copy sprite pixels into atlas
        memset(atlas, 0, atlas_w * atlas_h);
        for (int i = 0; i < num_sprites; i++) {
            copy_sprite_to_atlas(atlas, atlas_w, atlas_h,
                                rects[i].x, rects[i].y,
                                &sprites[i]);
        }
    }

    free(rects);
    free(nodes);

    return success;
}
```

### Multi-Pass Packing

```c
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

// Pack multiple batches into the same atlas
void multi_pass_packing(void) {
    stbrp_node nodes[1024];
    stbrp_context context;

    stbrp_init_target(&context, 1024, 1024, nodes, 1024);

    // First batch
    stbrp_rect batch1[10];
    // ... fill batch1 ...
    stbrp_pack_rects(&context, batch1, 10);

    // Second batch (continues packing into same target)
    stbrp_rect batch2[5];
    // ... fill batch2 ...
    stbrp_pack_rects(&context, batch2, 5);

    // Note: Single call with all rects may pack better than multiple calls
}
```

### Using Best-Fit Heuristic

```c
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

void pack_with_best_fit(void) {
    stbrp_node nodes[512];
    stbrp_context context;

    stbrp_init_target(&context, 512, 512, nodes, 512);

    // Use Best-Fit heuristic (slower but may pack better for some data)
    stbrp_setup_heuristic(&context, STBRP_HEURISTIC_Skyline_BF_sortHeight);

    stbrp_rect rects[100];
    // ... fill rects ...

    stbrp_pack_rects(&context, rects, 100);
}
```

## Integration with stb_truetype

This library is designed to work with stb_truetype.h for font atlas generation:

```c
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// stb_truetype's PackBegin/PackFontRanges/PackEnd use stb_rect_pack internally
void create_font_atlas(void) {
    stbtt_pack_context spc;
    unsigned char atlas[512 * 512];

    stbtt_PackBegin(&spc, atlas, 512, 512, 0, 1, NULL);
    // ... pack fonts ...
    stbtt_PackEnd(&spc);
}
```

## Algorithm Notes

The library uses the **Skyline Bottom-Left** algorithm:
- Maintains a "skyline" representing the top edge of packed rectangles
- Places new rectangles at the lowest available position
- Efficient O(n) per rectangle placement

**Heuristics:**
- `Skyline_BL_sortHeight`: Faster, good general-purpose packing
- `Skyline_BF_sortHeight`: Slower (~2x), may reduce wasted space

## VOS/TCC Compatibility Notes

### Compatible Features
- All packing functions work correctly with TCC
- No dynamic memory allocation (user provides buffers)
- Uses only qsort and assert from stdlib

### Requirements
- Include `<stdlib.h>` for qsort
- Include `<assert.h>` for assert (or define STBRP_ASSERT)

### Recommended Configuration for VOS

```c
// For VOS/TCC compatibility
#include <stdlib.h>
#include <assert.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
```

### Memory Considerations
- Node array must remain valid during pack_rects calls
- Stack allocation of nodes is fine for reasonable atlas sizes
- For large atlases, consider heap allocation
- Optimal node count equals target width

### Custom Sort Function

```c
// If qsort is unavailable, provide custom sort
void my_qsort(void *base, size_t num, size_t size,
              int (*compar)(const void *, const void *));

#define STBRP_SORT my_qsort
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
```
