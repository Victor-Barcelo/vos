# stb_perlin.h - Perlin Noise

## Overview

**stb_perlin.h** is a single-header library implementing Ken Perlin's revised noise function from 2002. It provides 3D Perlin noise generation with optional wrapping and seeding, plus common fractal noise functions for procedural content generation.

## Source

- **Original Repository:** https://github.com/nothings/stb
- **File:** stb_perlin.h
- **Version:** 0.5
- **Author:** Sean Barrett

## License

This software is dual-licensed:
- **Public Domain** (www.unlicense.org)
- **MIT License**

Choose whichever you prefer.

## Features

- Ken Perlin's improved noise algorithm (2002 version)
- 3D noise generation with continuous random values
- Wraparound noise at powers of two
- Multiple seed support for variation
- Fractal noise functions:
  - Fractional Brownian Motion (fBm)
  - Ridge noise
  - Turbulence noise
- Non-power-of-two wrapping support
- No dynamic memory allocation
- Single precision floating point

## API Reference

### Compile-Time Options

| Macro | Description |
|-------|-------------|
| `STB_PERLIN_IMPLEMENTATION` | Define in ONE file to include implementation |

### Core Noise Functions

```c
float stb_perlin_noise3(float x, float y, float z,
                        int x_wrap, int y_wrap, int z_wrap);
```

Computes a random value at coordinate (x, y, z). Returns values in approximately [-1, 1] range.

**Parameters:**
- `x, y, z` - 3D coordinates
- `x_wrap, y_wrap, z_wrap` - Wrap periods (must be powers of 2, or 0 for no wrapping)

**Notes:**
- Adjacent values are continuous
- Noise fluctuates with period 1 (integer points have unrelated values)
- Always wraps every 256 due to implementation

```c
float stb_perlin_noise3_seed(float x, float y, float z,
                             int x_wrap, int y_wrap, int z_wrap, int seed);
```

Same as above, but with a seed parameter for different noise variations.

**Parameters:**
- `seed` - Currently only bottom 8 bits are used

```c
float stb_perlin_noise3_wrap_nonpow2(float x, float y, float z,
                                     int x_wrap, int y_wrap, int z_wrap,
                                     unsigned char seed);
```

Noise function supporting non-power-of-two wrap values.

### Fractal Noise Functions

```c
float stb_perlin_fbm_noise3(float x, float y, float z,
                            float lacunarity, float gain, int octaves);
```

Fractional Brownian Motion - sums multiple octaves of noise for natural-looking patterns.

**Parameters:**
- `lacunarity` - Spacing between octaves (typically ~2.0)
- `gain` - Amplitude multiplier per octave (typically 0.5)
- `octaves` - Number of noise layers to sum

```c
float stb_perlin_ridge_noise3(float x, float y, float z,
                              float lacunarity, float gain, float offset, int octaves);
```

Ridge noise - creates sharp ridges, useful for mountain ranges.

**Parameters:**
- `offset` - Used to invert ridges (typically 1.0 or higher)

```c
float stb_perlin_turbulence_noise3(float x, float y, float z,
                                   float lacunarity, float gain, int octaves);
```

Turbulence noise - uses absolute value of noise for cloud/smoke effects.

## Usage Examples

### Basic Noise

```c
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

// Generate a simple 2D noise texture
void generate_noise_texture(float *pixels, int width, int height, float scale) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float nx = x * scale;
            float ny = y * scale;

            // Use z=0 for 2D noise
            float noise = stb_perlin_noise3(nx, ny, 0, 0, 0, 0);

            // Convert from [-1,1] to [0,1]
            pixels[y * width + x] = (noise + 1.0f) * 0.5f;
        }
    }
}
```

### Tileable Noise

```c
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

// Generate tileable noise that wraps at 256x256
void generate_tileable_noise(float *pixels, int size) {
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            // Scale to match wrap period
            float nx = (float)x / size * 256.0f;
            float ny = (float)y / size * 256.0f;

            // Wrap at 256 (power of 2)
            float noise = stb_perlin_noise3(nx, ny, 0, 256, 256, 0);
            pixels[y * size + x] = (noise + 1.0f) * 0.5f;
        }
    }
}
```

### Terrain Generation with fBm

```c
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

// Generate terrain heightmap using fBm
void generate_terrain(float *heightmap, int width, int height) {
    float scale = 0.01f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float noise = stb_perlin_fbm_noise3(
                x * scale, y * scale, 0,
                lacunarity, gain, octaves
            );
            heightmap[y * width + x] = noise;
        }
    }
}
```

### Mountain Ridges

```c
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

// Generate mountain-like ridges
void generate_mountains(float *heightmap, int width, int height) {
    float scale = 0.005f;
    int octaves = 6;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float offset = 1.0f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float noise = stb_perlin_ridge_noise3(
                x * scale, y * scale, 0,
                lacunarity, gain, offset, octaves
            );
            heightmap[y * width + x] = noise;
        }
    }
}
```

### Animated Clouds

```c
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

// Generate animated cloud texture
void generate_clouds(float *pixels, int width, int height, float time) {
    float scale = 0.02f;
    int octaves = 4;
    float lacunarity = 2.0f;
    float gain = 0.5f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float noise = stb_perlin_turbulence_noise3(
                x * scale, y * scale, time * 0.1f,
                lacunarity, gain, octaves
            );
            pixels[y * width + x] = noise;
        }
    }
}
```

## Typical Parameter Values

| Parameter | Typical Value | Description |
|-----------|--------------|-------------|
| octaves | 4-8 | More octaves = more detail, but slower |
| lacunarity | 2.0 | Use exactly 2.0 for tileable output |
| gain | 0.5 | Also called "persistence" |
| offset | 1.0 | For ridge noise, may need adjustment |
| scale | 0.01-0.1 | Depends on input coordinate range |

## VOS/TCC Compatibility Notes

### Compatible Features
- All noise functions work correctly with TCC
- No dynamic memory allocation
- Uses only standard math functions (fabs from math.h)

### Requirements
- Include `<math.h>` before or along with the header
- Single precision float operations only

### Recommended Configuration for VOS

```c
// For VOS/TCC compatibility
#include <math.h>
#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"
```

### Performance Considerations
- Each fractal function calls the base noise function `octaves` times
- For real-time applications, consider:
  - Reducing octave count
  - Pre-computing noise into textures
  - Using lower resolution and interpolating
- The algorithm is CPU-bound; consider threading for large areas
