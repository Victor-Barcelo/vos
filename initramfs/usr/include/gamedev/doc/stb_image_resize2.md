# stb_image_resize2.h

High-performance image resizing library with SIMD support.

## Source

- **Repository**: https://github.com/nothings/stb
- **File**: stb_image_resize2.h
- **Version**: 2.17
- **Authors**: Jeff Roberts (v2), Jorge L Rodriguez (v1), Sean Barrett (API)

## License

Dual-licensed (choose one):
- **MIT License** - Copyright (c) 2017 Sean Barrett
- **Public Domain** (Unlicense) - www.unlicense.org

## Description

stb_image_resize2 is a public domain image resizing library supporting scaling and translation (no rotations or shears). It features SSE2, AVX, AVX2, NEON, and WASM SIMD support, with deterministic output across platforms. Version 2 is 2-40x faster than version 1.

## Features

- **High Performance**: 2-5x faster without SIMD, 4-12x faster with SIMD
- **SIMD Support**: SSE2, AVX, AVX2, NEON, WASM SIMD
- **Deterministic**: Consistent results across platforms (x64, ARM, scalar)
- **Threading**: Extended API supports multi-threaded resizing
- **Alpha Handling**: Proper premultiplied/non-premultiplied alpha support
- **Multiple Data Types**: uint8, uint16, float, half-float
- **Color Space Aware**: sRGB and linear color space support
- **Flexible Filters**: Box, triangle, cubic, Mitchell, Catmull-Rom, point sampling
- **Edge Modes**: Clamp, reflect, wrap, zero
- **Negative Strides**: Supports flipped/inverted images
- **Built-in Profiling**: Optional performance profiling

## API Levels

### Easy API (Simple)

```c
// Downsamples with Mitchell filter, upsamples with cubic, clamps to edge

// sRGB color space (typical for photos)
unsigned char *stbir_resize_uint8_srgb(
    const unsigned char *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
    unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
    stbir_pixel_layout pixel_type);

// Linear color space
unsigned char *stbir_resize_uint8_linear(
    const unsigned char *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
    unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
    stbir_pixel_layout pixel_type);

// Float data
float *stbir_resize_float_linear(
    const float *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
    float *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
    stbir_pixel_layout pixel_type);
```

### Medium API

```c
// Full control over data type, edge mode, and filter
void *stbir_resize(
    const void *input_pixels, int input_w, int input_h, int input_stride_in_bytes,
    void *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
    stbir_pixel_layout pixel_layout, stbir_datatype data_type,
    stbir_edge edge, stbir_filter filter);
```

### Extended API

For advanced features like threading, callbacks, and multi-frame processing:
- Use `STBIR_RESIZE` structure
- Call `stbir_resize_init()` then setter functions
- Optionally pre-build samplers with `stbir_build_samplers()`
- Resize with `stbir_resize_extended()`
- Free samplers with `stbir_free_samplers()`

## Types and Enums

### Pixel Layouts

```c
typedef enum {
    STBIR_1CHANNEL = 1,
    STBIR_2CHANNEL = 2,
    STBIR_RGB      = 3,
    STBIR_BGR      = 0,
    STBIR_4CHANNEL = 5,

    // Non-premultiplied alpha (applies alpha weighting)
    STBIR_RGBA = 4, STBIR_BGRA = 6, STBIR_ARGB = 7, STBIR_ABGR = 8,
    STBIR_RA = 9, STBIR_AR = 10,

    // Premultiplied alpha (no alpha weighting)
    STBIR_RGBA_PM = 11, STBIR_BGRA_PM = 12, STBIR_ARGB_PM = 13,
    STBIR_ABGR_PM = 14, STBIR_RA_PM = 15, STBIR_AR_PM = 16,
} stbir_pixel_layout;
```

### Data Types

```c
typedef enum {
    STBIR_TYPE_UINT8            = 0,
    STBIR_TYPE_UINT8_SRGB       = 1,
    STBIR_TYPE_UINT8_SRGB_ALPHA = 2,  // Alpha also in sRGB (unusual)
    STBIR_TYPE_UINT16           = 3,
    STBIR_TYPE_FLOAT            = 4,
    STBIR_TYPE_HALF_FLOAT       = 5
} stbir_datatype;
```

### Edge Modes

```c
typedef enum {
    STBIR_EDGE_CLAMP   = 0,  // Default, fastest
    STBIR_EDGE_REFLECT = 1,
    STBIR_EDGE_WRAP    = 2,  // Slower, uses more memory
    STBIR_EDGE_ZERO    = 3,
} stbir_edge;
```

### Filters

```c
typedef enum {
    STBIR_FILTER_DEFAULT      = 0,  // Auto-select
    STBIR_FILTER_BOX          = 1,  // Trapezoid
    STBIR_FILTER_TRIANGLE     = 2,  // Bilinear
    STBIR_FILTER_CUBICBSPLINE = 3,  // Mitchell B=1,C=0
    STBIR_FILTER_CATMULLROM   = 4,  // Interpolating cubic
    STBIR_FILTER_MITCHELL     = 5,  // Mitchell B=1/3,C=1/3
    STBIR_FILTER_POINT_SAMPLE = 6,  // Nearest neighbor
    STBIR_FILTER_OTHER        = 7,  // User callback
} stbir_filter;
```

### Configuration Macros

| Macro | Description |
|-------|-------------|
| `STB_IMAGE_RESIZE_IMPLEMENTATION` | Include implementation |
| `STB_IMAGE_RESIZE_STATIC` | Make functions static |
| `STBIR_MALLOC(size, user_data)` | Override malloc |
| `STBIR_FREE(ptr, user_data)` | Override free |
| `STBIR_ASSERT(x)` | Override assert |
| `STBIR_PROFILE` | Enable profiling |
| `STBIR_NO_SIMD` | Disable all SIMD |
| `STBIR_SSE2` / `STBIR_AVX` / `STBIR_AVX2` | Force SIMD mode |
| `STBIR_NO_AVX` / `STBIR_NO_AVX2` | Disable specific SIMD |
| `STBIR_USE_FMA` | Enable FMA instructions |

## Usage Example

### Basic Resize

```c
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

// Resize RGBA image from 800x600 to 400x300
unsigned char input[800 * 600 * 4];
unsigned char output[400 * 300 * 4];

stbir_resize_uint8_srgb(
    input, 800, 600, 0,    // 0 = packed stride
    output, 400, 300, 0,
    STBIR_RGBA);
```

### With Output Allocation

```c
// Pass NULL for output to auto-allocate
unsigned char *result = stbir_resize_uint8_srgb(
    input, 800, 600, 0,
    NULL, 400, 300, 0,
    STBIR_RGBA);
// ... use result ...
free(result);
```

### Medium API with Options

```c
unsigned char *result = stbir_resize(
    input, 800, 600, 0,
    NULL, 400, 300, 0,
    STBIR_RGBA,
    STBIR_TYPE_UINT8_SRGB,
    STBIR_EDGE_WRAP,         // Wrap edges (for textures)
    STBIR_FILTER_MITCHELL);  // Explicit filter
```

### Flipped Image (Negative Stride)

```c
// Point to last row, use negative stride
unsigned char *last_row = input + (height - 1) * stride;
stbir_resize_uint8_srgb(
    last_row, 800, 600, -stride,  // Negative stride
    output, 400, 300, 0,
    STBIR_RGBA);
```

## Alpha Channel Handling

Three scenarios:

1. **Non-premultiplied with weighting** (default for RGBA/BGRA/etc.):
   - Library applies alpha weighting during filtering
   - Mathematically correct for transparency

2. **Non-premultiplied without weighting** (use _PM or _NO_AW suffixes):
   - Treat alpha as regular channel
   - Faster but may introduce color artifacts

3. **Premultiplied** (_PM suffix):
   - Input already premultiplied
   - ~2x faster than non-premultiplied

## Performance Tips

1. **Use STBIR_4CHANNEL with UINT8 for fastest mode** when alpha doesn't need special handling
2. **Enable SIMD** (default for 64-bit targets)
3. **Avoid STBIR_EDGE_WRAP** if possible (slower)
4. **Use premultiplied alpha** (_PM layouts) for best performance
5. **Pre-build samplers** when resizing multiple frames at same resolution

## VOS/TCC Compatibility Notes

1. **Standard Headers Required**:
   - `<stddef.h>` - for size_t
   - `<stdint.h>` - for fixed-width integers (non-MSVC)
   - `<math.h>` - for ceil, floor (in scalar mode)

2. **TCC Specific**:
   - SIMD is disabled automatically for TCC (as of v2.13)
   - Will use scalar fallback
   - All core functionality works

3. **Memory**: Single malloc/free per resize (extended API can reuse allocations)

4. **Large Resizes**: Temp memory can exceed 2GB for very large images

### TCC Configuration

```c
// TCC automatically gets scalar mode, no special config needed
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
```

### Minimal Memory Configuration

```c
// Custom allocators for embedded use
#define STBIR_MALLOC(size, user_data) my_malloc(size)
#define STBIR_FREE(ptr, user_data) my_free(ptr)
#define STBIR_ASSERT(x)  // Disable asserts

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
```

## Determinism Notes

- Deterministic across platforms when using same SIMD level
- Requires fast-math OFF (`/fp:precise` or equivalent)
- Requires FP contracting OFF (`-ffp-contract=off` for Clang)
- NaN values are NOT deterministic
- FMA vs non-FMA targets will differ

## Dependencies

- `<stddef.h>` - for size_t
- `<stdint.h>` - for fixed-width integers (non-MSVC)
- `<math.h>` - for ceilf, floorf (scalar mode, can override)
- `<assert.h>` - for assert (can override)
- `<stdlib.h>` - for malloc, free (can override)
