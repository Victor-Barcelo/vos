# vos_gamedev.h

## Overview

**vos_gamedev.h** is a VOS-specific compatibility header that ensures all game development libraries work correctly when compiled with TCC inside VOS. Include this header **before** any other gamedev library to automatically configure platform detection, disable unsupported features (like SIMD), and provide VOS-specific helpers.

## Source

- **Location**: `/gameResources/vos_gamedev.h`
- **Author**: VOS Project
- **License**: Public Domain

## Features

- Automatic VOS platform detection (`__VOS__` define)
- TCC compatibility settings (disables SIMD for libraries that use it)
- Physac configuration for standalone mode (no raylib dependency)
- Time helper function `vos_get_time_ms()` for game timing
- Memory allocation wrappers for customization
- Quick reference for including all libraries

## Platform Detection

The header automatically detects VOS when:
- `__TINYC__` is defined (TCC compiler)
- Neither `__linux__` nor `_WIN32` is defined

```c
#if defined(__TINYC__) && !defined(__linux__) && !defined(_WIN32)
# define __VOS__
#endif
```

## Compatibility Settings

### SIMD Disabled
```c
#define HANDMADE_MATH_NO_SIMD  // TCC doesn't support intrinsics
```

### Physac Configuration
```c
#define PHYSAC_STANDALONE      // No raylib dependency
#define PHYSAC_NO_THREADS      // VOS doesn't have pthreads
```

## API Reference

### Time Functions

#### `vos_get_time_ms()`
Returns the current time in milliseconds.

```c
static inline double vos_get_time_ms(void);
```

**Returns**: Current time in milliseconds since system boot.

**Example**:
```c
double start = vos_get_time_ms();
// ... do work ...
double elapsed = vos_get_time_ms() - start;
printf("Elapsed: %.2f ms\n", elapsed);
```

### Memory Allocation Macros

Customize memory allocation by defining these before including the header:

```c
#define VOS_MALLOC(sz)       malloc(sz)
#define VOS_FREE(ptr)        free(ptr)
#define VOS_REALLOC(ptr, sz) realloc(ptr, sz)
```

## Usage

### Basic Usage

Always include `vos_gamedev.h` first:

```c
#include "vos_gamedev.h"

// Now include any gamedev library
#include "linmath.h"
```

### With Single-Header Libraries

```c
#include "vos_gamedev.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define PHYSAC_IMPLEMENTATION
#include "physac.h"
```

### Library Quick Reference

| Category | Include Pattern |
|----------|-----------------|
| **Math** | `#include "linmath.h"` or `hypatia.h` or `HandmadeMath.h` |
| **Physics** | `#define PHYSAC_IMPLEMENTATION` then `#include "physac.h"` |
| **Collision** | `#define SATC_IMPLEMENTATION` then `#include "satc.h"` |
| **ECS** | `#define ECS_IMPLEMENTATION` then `#include "ecs.h"` |
| **FSM** | `#include "sm.h"` or `stateMachine.h` or `hsm.h` |
| **Pathfinding** | `#include "AStar.h"` (link with AStar.c) |
| **Easing** | `#include "easing.h"` (link with easing.c) |
| **Random** | `#include "pcg_basic.h"` (link with pcg_basic.c) |
| **Noise** | `#define STB_PERLIN_IMPLEMENTATION` then `#include "stb_perlin.h"` |
| **Data Structures** | `#define STB_DS_IMPLEMENTATION` then `#include "stb_ds.h"` |

## Complete Example

```c
// game.c - Complete VOS game setup
#include "vos_gamedev.h"

// Math
#include "linmath.h"

// Random numbers
#include "pcg_basic.h"

// Data structures
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// Physics
#define PHYSAC_IMPLEMENTATION
#include "physac.h"

int main(void) {
    // Initialize RNG
    pcg32_random_t rng;
    pcg32_srandom_r(&rng, 12345, 67890);

    // Track time
    double last_time = vos_get_time_ms();

    // Game loop
    while (1) {
        double current_time = vos_get_time_ms();
        double dt = (current_time - last_time) / 1000.0;
        last_time = current_time;

        // Update physics
        UpdatePhysics();

        // Game logic here...
    }

    return 0;
}
```

Compile with:
```bash
tcc game.c pcg_basic.c -lm -o game
```

## VOS/TCC Compatibility Notes

- **SIMD**: Automatically disabled - TCC doesn't support SSE/AVX intrinsics
- **Threading**: Disabled for Physac - VOS doesn't have pthreads
- **Time**: Uses `sys_uptime()` syscall which returns milliseconds
- **Standard Library**: Full C standard library available via VOS libc

## Files This Header Configures

| Library | Configuration |
|---------|---------------|
| HandmadeMath.h | `HANDMADE_MATH_NO_SIMD` |
| physac.h | `PHYSAC_STANDALONE`, `PHYSAC_NO_THREADS`, VOS timing |
| stb_image_resize2.h | Auto-detects TCC (no config needed) |

## See Also

- Individual library documentation in `/gameResources/doc/`
- Example code in `/gameResources/gameExamples/`
