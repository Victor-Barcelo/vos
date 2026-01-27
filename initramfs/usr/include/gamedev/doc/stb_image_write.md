# stb_image_write.h

Image file writing library for PNG, BMP, TGA, JPEG, and HDR formats.

## Source

- **Repository**: https://github.com/nothings/stb
- **File**: stb_image_write.h
- **Version**: 1.16
- **Author**: Sean Barrett

## License

Dual-licensed (choose one):
- **MIT License** - Copyright (c) 2017 Sean Barrett
- **Public Domain** (Unlicense) - www.unlicense.org

## Description

stb_image_write is a single-header library for writing images to C stdio or custom callbacks. It supports PNG, BMP, TGA, JPEG, and HDR output formats. The library prioritizes source code compactness and simplicity over optimal file size or runtime performance.

## Features

- **Multiple Formats**: PNG, BMP, TGA, JPEG, HDR output
- **Flexible I/O**: Write to files or custom callbacks
- **PNG Options**: Configurable compression level and filter selection
- **TGA Options**: Optional RLE compression
- **JPEG Options**: Quality setting (1-100)
- **Vertical Flip**: Built-in image flipping option
- **Unicode Support**: Windows UTF-8 filename support
- **Customizable**: Replace memory allocators and compression functions

## API Reference

### File-Based Writers

```c
// Write PNG (supports stride for non-contiguous data)
int stbi_write_png(char const *filename, int w, int h, int comp,
                   const void *data, int stride_in_bytes);

// Write BMP (expands grayscale to RGB, no alpha support)
int stbi_write_bmp(char const *filename, int w, int h, int comp,
                   const void *data);

// Write TGA (supports RLE compression)
int stbi_write_tga(char const *filename, int w, int h, int comp,
                   const void *data);

// Write JPEG (quality: 1-100, ignores alpha)
int stbi_write_jpg(char const *filename, int x, int y, int comp,
                   const void *data, int quality);

// Write HDR (expects linear float data)
int stbi_write_hdr(char const *filename, int w, int h, int comp,
                   const float *data);
```

### Callback-Based Writers

```c
typedef void stbi_write_func(void *context, void *data, int size);

int stbi_write_png_to_func(stbi_write_func *func, void *context,
                           int w, int h, int comp, const void *data,
                           int stride_in_bytes);

int stbi_write_bmp_to_func(stbi_write_func *func, void *context,
                           int w, int h, int comp, const void *data);

int stbi_write_tga_to_func(stbi_write_func *func, void *context,
                           int w, int h, int comp, const void *data);

int stbi_write_jpg_to_func(stbi_write_func *func, void *context,
                           int x, int y, int comp, const void *data,
                           int quality);

int stbi_write_hdr_to_func(stbi_write_func *func, void *context,
                           int w, int h, int comp, const float *data);
```

### Utility Functions

```c
// Flip images vertically on write
void stbi_flip_vertically_on_write(int flag);

// Windows UTF-8 filename conversion (when STBIW_WINDOWS_UTF8 defined)
int stbiw_convert_wchar_to_utf8(char *buffer, size_t bufferlen,
                                 const wchar_t *input);
```

### Global Configuration Variables

```c
int stbi_write_tga_with_rle;          // Default: 1 (enabled)
int stbi_write_png_compression_level;  // Default: 8 (0-9)
int stbi_write_force_png_filter;       // Default: -1 (auto), or 0-5
```

### Configuration Macros

| Macro | Description |
|-------|-------------|
| `STB_IMAGE_WRITE_IMPLEMENTATION` | Include implementation |
| `STB_IMAGE_WRITE_STATIC` | Make all functions static |
| `STBI_WRITE_NO_STDIO` | Disable file I/O (also disables HDR) |
| `STBIW_WINDOWS_UTF8` | Enable Windows UTF-8 filename support |
| `STBIW_ASSERT(x)` | Override assert |
| `STBIW_MALLOC(sz)` | Override malloc |
| `STBIW_REALLOC(p,newsz)` | Override realloc |
| `STBIW_FREE(p)` | Override free |
| `STBIW_MEMMOVE(a,b,sz)` | Override memmove |
| `STBIW_ZLIB_COMPRESS` | Custom PNG compression function |

### Pixel Data Format

| Components | Format |
|------------|--------|
| 1 | Grayscale (Y) |
| 2 | Grayscale + Alpha (YA) |
| 3 | RGB |
| 4 | RGBA |

Data is stored:
- Left-to-right, top-to-bottom
- 8 bits per channel (except HDR which uses float)
- First byte of data = top-left pixel

## Usage Example

### Basic File Writing

```c
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Write a 256x256 RGB image
unsigned char pixels[256 * 256 * 3];
// ... fill pixels ...

// PNG (with stride = width * components for contiguous data)
stbi_write_png("output.png", 256, 256, 3, pixels, 256 * 3);

// BMP
stbi_write_bmp("output.bmp", 256, 256, 3, pixels);

// TGA
stbi_write_tga("output.tga", 256, 256, 3, pixels);

// JPEG (quality 90)
stbi_write_jpg("output.jpg", 256, 256, 3, pixels, 90);

// HDR (float data)
float hdr_pixels[256 * 256 * 3];
stbi_write_hdr("output.hdr", 256, 256, 3, hdr_pixels);
```

### Callback-Based Writing

```c
void my_write_func(void *context, void *data, int size) {
    FILE *f = (FILE *)context;
    fwrite(data, 1, size, f);
}

FILE *f = fopen("output.png", "wb");
stbi_write_png_to_func(my_write_func, f, 256, 256, 3, pixels, 256 * 3);
fclose(f);
```

### Custom Compression

```c
// Custom zlib-style compress function
unsigned char *my_compress(unsigned char *data, int data_len,
                           int *out_len, int quality) {
    // Your compression implementation
    // Must return heap-allocated buffer (freed with STBIW_FREE)
}

#define STBIW_ZLIB_COMPRESS my_compress
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```

### Sub-Rectangle Writing (PNG only)

```c
// Write a 100x100 sub-rectangle from a 512x512 image
unsigned char full_image[512 * 512 * 4];
unsigned char *sub_rect = &full_image[(100 * 512 + 100) * 4];  // Start at (100,100)
stbi_write_png("sub.png", 100, 100, 4, sub_rect, 512 * 4);  // Stride = full width
```

## Format-Specific Notes

### PNG
- Output is 20-50% larger than optimized PNG encoders
- Supports non-contiguous data via stride parameter
- Compression level 0-9 (default 8)
- Filter modes 0-5 available

### BMP
- Expands grayscale to RGB
- Does not output alpha channel
- No stride support (data must be contiguous)

### TGA
- RLE compression enabled by default
- Disable with `stbi_write_tga_with_rle = 0`

### JPEG
- Baseline JPEG only (no progressive)
- Quality 1-100 (higher = better quality, larger file)
- Alpha channel is ignored

### HDR
- Expects linear float data (0.0 to 1.0+)
- Alpha channel is discarded
- Grayscale is replicated to RGB
- Requires stdio (disabled if STBI_WRITE_NO_STDIO)

## VOS/TCC Compatibility Notes

1. **Standard Headers Required**:
   - `<stdio.h>` - for file I/O (unless STBI_WRITE_NO_STDIO)
   - `<stdlib.h>` - for malloc/free
   - `<stdarg.h>` - for va_list
   - `<string.h>` - for memmove
   - `<math.h>` - for math functions
   - `<assert.h>` - for assert (can override)

2. **Strict Aliasing Warning**: May not work correctly with strict-aliasing optimizations

3. **TCC Compatible**: All features work with TCC

4. **No SIMD**: Pure C implementation

### Minimal Configuration for Embedded

```c
// Disable file I/O, use callbacks only
#define STBI_WRITE_NO_STDIO

// Custom memory allocation
#define STBIW_MALLOC(sz) my_malloc(sz)
#define STBIW_REALLOC(p,newsz) my_realloc(p, newsz)
#define STBIW_FREE(p) my_free(p)

// Disable assert
#define STBIW_ASSERT(x)

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```

## Return Values

All write functions return:
- **Non-zero (1)**: Success
- **Zero (0)**: Failure

## Dependencies

- `<stdio.h>` - for file I/O (optional)
- `<stdlib.h>` - for malloc, realloc, free
- `<stdarg.h>` - for va_list
- `<string.h>` - for memmove
- `<math.h>` - for math functions
- `<assert.h>` - for assert (optional)
