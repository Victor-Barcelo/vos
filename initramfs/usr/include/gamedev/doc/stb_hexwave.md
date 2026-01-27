# stb_hexwave.h

Flexible anti-aliased (bandlimited) digital audio oscillator library.

## Source

- **Repository**: https://github.com/nothings/stb
- **File**: stb_hexwave.h
- **Version**: 0.5
- **Author**: Sean Barrett
- **Initial Release**: 2021-04-01

## License

Dual-licensed (choose one):
- **MIT License** - Copyright (c) 2017 Sean Barrett
- **Public Domain** (Unlicense) - www.unlicense.org

## Description

stb_hexwave generates artifact-free morphable digital waveforms with various spectra. It creates waveforms from six line segments characterized by three parameters, using BLEP (Band-Limited Step) and BLAMP (Band-Limited ramp) techniques for anti-aliasing.

The library handles only waveform generation - envelopes, LFO effects, and mixing multiple voices are left to the user.

## Features

- **Bandlimited Synthesis**: Uses BLEP and BLAMP (not polyBLEP) for anti-aliasing
- **Morphable Waveforms**: Smooth transitions between different wave shapes
- **Zero DC Offset**: All shapes are designed with zero DC offset
- **Artifact-Free**: Frequency sweeps and LFO modulation without aliasing
- **Thread-Safe Changes**: Waveform changes can be made from different threads
- **Memory Efficient**: Optional pre-allocated buffers, no runtime allocation after init
- **Classic Waveforms**: Can produce sawtooth, square, triangle, and more

## API Reference

### Types

```c
typedef struct HexWave HexWave;

typedef struct {
    int   reflect;
    float peak_time;
    float zero_wait;
    float half_height;
} HexWaveParameters;

struct HexWave {
    float t, prev_dt;
    HexWaveParameters current, pending;
    int have_pending;
    float buffer[STB_HEXWAVE_MAX_BLEP_LENGTH];
};
```

### Functions

```c
// Initialize the library (call once)
void hexwave_init(int width, int oversample, float *user_buffer);
// width: BLEP size 4..64 (larger = less aliasing, more memory/CPU)
// oversample: 2+ subsample positions (larger = less noise, more memory)
// user_buffer: NULL for dynamic allocation, or pre-allocated buffer

// Shutdown and free resources
void hexwave_shutdown(float *user_buffer);

// Create an oscillator
void hexwave_create(HexWave *hex, int reflect, float peak_time,
                    float half_height, float zero_wait);

// Change oscillator parameters (takes effect at cycle boundary)
void hexwave_change(HexWave *hex, int reflect, float peak_time,
                    float half_height, float zero_wait);

// Generate audio samples
void hexwave_generate_samples(float *output, int num_samples,
                              HexWave *hex, float freq);
// freq = oscillator_frequency / sample_rate
```

### Configuration Macros

| Macro | Description |
|-------|-------------|
| `STB_HEXWAVE_IMPLEMENTATION` | Include implementation |
| `STB_HEXWAVE_STATIC` | Make functions static |
| `STB_HEXWAVE_MAX_BLEP_LENGTH` | Maximum BLEP width (default: 64) |
| `STB_HEXWAVE_NO_ALLOCATION` | Disable malloc/free |

## Waveform Parameters

All waveforms are constructed from six line segments controlled by three parameters:

| Parameter | Range | Description |
|-----------|-------|-------------|
| `reflect` | 0 or 1 | Symmetry mode of second half-cycle |
| `peak_time` | 0.0-1.0 | Position of peak within first half-cycle |
| `half_height` | any | Amplitude at half-cycle point |
| `zero_wait` | 0.0-1.0 | Duration at zero before rising |

### Classic Waveform Settings

| Waveform | reflect | peak_time | half_height | zero_wait |
|----------|---------|-----------|-------------|-----------|
| Sawtooth | 1 | 0 | 0 | 0 |
| Square | 1 | 0 | 1 | 0 |
| Triangle | 1 | 0.5 | 0 | 0 |
| Sawtooth (8va) | 1 | 0 | -1 | 0 |
| AlternatingSaw | 0 | 0 | 0 | 0 |
| Stairs | 0 | 0 | 1 | 0.5 |

## Usage Example

```c
// In ONE source file:
#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"

// Initialize library
hexwave_init(32, 16, NULL);  // width=32, oversample=16, dynamic alloc

// Create oscillator (triangle wave)
HexWave osc;
hexwave_create(&osc, 1, 0.5f, 0.0f, 0.0f);

// Generate samples at 440Hz, 44100 sample rate
float samples[1024];
float freq = 440.0f / 44100.0f;
hexwave_generate_samples(samples, 1024, &osc, freq);

// Morph to square wave
hexwave_change(&osc, 1, 0.0f, 1.0f, 0.0f);

// Generate more samples (morph happens at cycle boundary)
hexwave_generate_samples(samples, 1024, &osc, freq);

// Cleanup
hexwave_shutdown(NULL);
```

### Pre-allocated Buffer Example

```c
// Calculate buffer size: 16 * width * (oversample + 1)
#define WIDTH 32
#define OVERSAMPLE 16
float buffer[16 * WIDTH * (OVERSAMPLE + 1)];

hexwave_init(WIDTH, OVERSAMPLE, buffer);
// ... use library ...
hexwave_shutdown(buffer);
```

## Morphing Between Waveforms

### Simple Crossfades (one parameter)

| Start | End | Parameter to Sweep |
|-------|-----|-------------------|
| Triangle | Square | half_height: -1 to 1 (reflect=0) |
| Saw | Square | half_height: 0 to 1 (reflect=1) |

### Two-Parameter Morphs

| Start | End | peak_time | half_height |
|-------|-----|-----------|-------------|
| Square | Triangle | 0 to 0.5 | 1 to 0 |
| Square | Saw | 0 to 1 | 1 to any |
| Triangle | Saw | 0.5 to 1 | 0 to -1 |

## VOS/TCC Compatibility Notes

1. **Standard Headers Required**:
   - `<stdlib.h>` - for malloc/free (unless STB_HEXWAVE_NO_ALLOCATION)
   - `<string.h>` - for memset, memcpy, memmove
   - `<math.h>` - for sin, cos, fabs

2. **Floating Point**: Heavy use of float arithmetic; ensure FPU support

3. **No SIMD Required**: Pure C implementation, no platform-specific optimizations

4. **Memory Requirements**:
   - HexWave struct: ~260 bytes (with default MAX_BLEP_LENGTH=64)
   - Global tables: 16 * width * (oversample + 1) bytes

5. **TCC Compatible**: No compiler-specific features used

### For Embedded/No-Allocation Use

```c
#define STB_HEXWAVE_NO_ALLOCATION
#define STB_HEXWAVE_MAX_BLEP_LENGTH 32  // Reduce if memory constrained
#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"

// Must provide buffer
float my_buffer[16 * 32 * 17];  // width=32, oversample=16
hexwave_init(32, 16, my_buffer);
```

## Dependencies

- `<stdlib.h>` - malloc, free (optional with STB_HEXWAVE_NO_ALLOCATION)
- `<string.h>` - memset, memcpy, memmove
- `<math.h>` - sin, cos, fabs
