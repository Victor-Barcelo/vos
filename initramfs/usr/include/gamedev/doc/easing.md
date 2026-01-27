# AHEasing - Easing Functions Library

## Description

AHEasing is a library of easing functions for C, C++, and Objective-C. It provides a comprehensive set of interpolation functions commonly used in animations, transitions, and game development. The library is designed to be fast, portable, and mathematically clear.

Easing functions take a normalized time value (0.0 to 1.0) and return a progress value that creates smooth, natural-feeling motion rather than linear interpolation.

## Original Source

- **Repository**: [https://github.com/warrenm/AHEasing](https://github.com/warrenm/AHEasing)
- **Author**: Warren Moore
- **Inspiration**: Based on Robert Penner's easing equations

## License

The Unlicense (Public Domain)

```
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any means.
```

## Features

- 31 easing functions across 11 categories
- Pure C implementation with no dependencies (except math.h)
- Supports both float and double precision
- Clean, readable mathematical implementations
- Cross-platform compatible
- Zero runtime allocation

## API Reference

### Types

#### `AHFloat`
Platform-adaptive floating-point type:
- `double` on 64-bit platforms (when `__LP64__` is defined)
- `float` on 32-bit platforms
- Can be overridden by defining `AH_EASING_USE_DBL_PRECIS`

#### `AHEasingFunction`
Function pointer type for easing functions:
```c
typedef AHFloat (*AHEasingFunction)(AHFloat);
```

### Easing Functions

All functions accept a parameter `p` in the range [0, 1] and return a value representing progress.

#### Linear
| Function | Description |
|----------|-------------|
| `LinearInterpolation(p)` | No easing, linear interpolation: `y = x` |

#### Quadratic (p^2)
| Function | Description |
|----------|-------------|
| `QuadraticEaseIn(p)` | Accelerating from zero velocity |
| `QuadraticEaseOut(p)` | Decelerating to zero velocity |
| `QuadraticEaseInOut(p)` | Acceleration until halfway, then deceleration |

#### Cubic (p^3)
| Function | Description |
|----------|-------------|
| `CubicEaseIn(p)` | Accelerating from zero velocity |
| `CubicEaseOut(p)` | Decelerating to zero velocity |
| `CubicEaseInOut(p)` | Acceleration until halfway, then deceleration |

#### Quartic (p^4)
| Function | Description |
|----------|-------------|
| `QuarticEaseIn(p)` | Accelerating from zero velocity |
| `QuarticEaseOut(p)` | Decelerating to zero velocity |
| `QuarticEaseInOut(p)` | Acceleration until halfway, then deceleration |

#### Quintic (p^5)
| Function | Description |
|----------|-------------|
| `QuinticEaseIn(p)` | Accelerating from zero velocity |
| `QuinticEaseOut(p)` | Decelerating to zero velocity |
| `QuinticEaseInOut(p)` | Acceleration until halfway, then deceleration |

#### Sine Wave
| Function | Description |
|----------|-------------|
| `SineEaseIn(p)` | Accelerating using sine curve |
| `SineEaseOut(p)` | Decelerating using sine curve |
| `SineEaseInOut(p)` | Acceleration/deceleration using sine curve |

#### Circular
| Function | Description |
|----------|-------------|
| `CircularEaseIn(p)` | Accelerating using circular curve: `sqrt(1 - p^2)` |
| `CircularEaseOut(p)` | Decelerating using circular curve |
| `CircularEaseInOut(p)` | Acceleration/deceleration using circular curve |

#### Exponential (base 2)
| Function | Description |
|----------|-------------|
| `ExponentialEaseIn(p)` | Accelerating exponentially: `2^(10(x-1))` |
| `ExponentialEaseOut(p)` | Decelerating exponentially |
| `ExponentialEaseInOut(p)` | Exponential acceleration/deceleration |

#### Elastic (Damped Sine Wave)
| Function | Description |
|----------|-------------|
| `ElasticEaseIn(p)` | Elastic effect at start |
| `ElasticEaseOut(p)` | Elastic effect at end |
| `ElasticEaseInOut(p)` | Elastic effect at both ends |

#### Back (Overshooting)
| Function | Description |
|----------|-------------|
| `BackEaseIn(p)` | Overshoots then accelerates |
| `BackEaseOut(p)` | Overshoots then decelerates |
| `BackEaseInOut(p)` | Overshoots at both ends |

#### Bounce
| Function | Description |
|----------|-------------|
| `BounceEaseIn(p)` | Bounce effect at start |
| `BounceEaseOut(p)` | Bounce effect at end |
| `BounceEaseInOut(p)` | Bounce effect at both ends |

## Usage Examples

### Basic Animation Interpolation

```c
#include "easing.h"

// Animate a value from startVal to endVal over time
float animate(float startVal, float endVal, float t, AHEasingFunction easing) {
    // t should be in range [0, 1]
    float progress = easing(t);
    return startVal + (endVal - startVal) * progress;
}

// Usage
float x = animate(0.0f, 100.0f, 0.5f, QuadraticEaseOut);
```

### Sprite Movement

```c
#include "easing.h"

typedef struct {
    float x, y;
    float startX, startY;
    float targetX, targetY;
    float elapsed;
    float duration;
    AHEasingFunction easing;
} MovingSprite;

void updateSprite(MovingSprite *sprite, float dt) {
    sprite->elapsed += dt;

    if (sprite->elapsed >= sprite->duration) {
        sprite->x = sprite->targetX;
        sprite->y = sprite->targetY;
        return;
    }

    float t = sprite->elapsed / sprite->duration;
    float progress = sprite->easing(t);

    sprite->x = sprite->startX + (sprite->targetX - sprite->startX) * progress;
    sprite->y = sprite->startY + (sprite->targetY - sprite->startY) * progress;
}

// Create a sprite that bounces to its target
void moveSpriteWithBounce(MovingSprite *sprite, float targetX, float targetY, float duration) {
    sprite->startX = sprite->x;
    sprite->startY = sprite->y;
    sprite->targetX = targetX;
    sprite->targetY = targetY;
    sprite->elapsed = 0.0f;
    sprite->duration = duration;
    sprite->easing = BounceEaseOut;
}
```

### Color Fading

```c
#include "easing.h"

typedef struct { unsigned char r, g, b, a; } Color;

Color lerpColor(Color start, Color end, float t, AHEasingFunction easing) {
    float p = easing(t);
    Color result;
    result.r = (unsigned char)(start.r + (end.r - start.r) * p);
    result.g = (unsigned char)(start.g + (end.g - start.g) * p);
    result.b = (unsigned char)(start.b + (end.b - start.b) * p);
    result.a = (unsigned char)(start.a + (end.a - start.a) * p);
    return result;
}

// Smooth fade in
Color fadeIn(float t) {
    Color transparent = {255, 255, 255, 0};
    Color opaque = {255, 255, 255, 255};
    return lerpColor(transparent, opaque, t, SineEaseInOut);
}
```

### UI Menu Animation

```c
#include "easing.h"

typedef struct {
    float offsetX;
    float targetOffsetX;
    float animTime;
    int isAnimating;
} MenuPanel;

void animateMenuSlide(MenuPanel *menu, float targetX) {
    menu->targetOffsetX = targetX;
    menu->animTime = 0.0f;
    menu->isAnimating = 1;
}

void updateMenu(MenuPanel *menu, float dt) {
    if (!menu->isAnimating) return;

    menu->animTime += dt;
    float duration = 0.3f;  // 300ms animation

    if (menu->animTime >= duration) {
        menu->offsetX = menu->targetOffsetX;
        menu->isAnimating = 0;
    } else {
        float t = menu->animTime / duration;
        // Use BackEaseOut for a satisfying "snap" effect
        float startX = menu->offsetX;
        float progress = BackEaseOut(t);
        menu->offsetX = startX + (menu->targetOffsetX - startX) * progress;
    }
}
```

### Easing Function Selection by Name

```c
#include "easing.h"
#include <string.h>

AHEasingFunction getEasingByName(const char *name) {
    if (strcmp(name, "linear") == 0) return LinearInterpolation;
    if (strcmp(name, "quadIn") == 0) return QuadraticEaseIn;
    if (strcmp(name, "quadOut") == 0) return QuadraticEaseOut;
    if (strcmp(name, "quadInOut") == 0) return QuadraticEaseInOut;
    if (strcmp(name, "cubicIn") == 0) return CubicEaseIn;
    if (strcmp(name, "cubicOut") == 0) return CubicEaseOut;
    if (strcmp(name, "cubicInOut") == 0) return CubicEaseInOut;
    if (strcmp(name, "bounceOut") == 0) return BounceEaseOut;
    if (strcmp(name, "elasticOut") == 0) return ElasticEaseOut;
    if (strcmp(name, "backOut") == 0) return BackEaseOut;
    // ... add more as needed
    return LinearInterpolation;  // default
}
```

## VOS/TCC Compatibility Notes

### Compilation
The library compiles cleanly with TCC:

```c
#include "easing.h"
#include "easing.c"
```

### Dependencies
- `<math.h>` - for sin, cos, pow, sqrt

### Precision Configuration
By default on VOS (32-bit), the library uses `float` precision. To force double precision:

```c
#define AH_EASING_USE_DBL_PRECIS
#include "easing.h"
```

### Math Constants
The library uses `M_PI` and `M_PI_2` from `<math.h>`. If these are not defined on your system, add:

```c
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif
```

### Performance Notes
- All functions are simple mathematical operations
- No memory allocation
- Consider caching easing results for repeated animations
- For performance-critical code, inline frequently used functions

### Common Use Cases in VOS Games
- **Menu transitions**: Use `BackEaseOut` or `CubicEaseOut`
- **Jump arcs**: Use `QuadraticEaseOut` for natural gravity feel
- **Hit reactions**: Use `ElasticEaseOut` for springy effects
- **Fade in/out**: Use `SineEaseInOut` for smooth opacity changes
- **Countdowns**: Use `ExponentialEaseIn` for dramatic tension

### Visual Reference
```
EaseIn:    Slow start, fast end     _____/
EaseOut:   Fast start, slow end     /-----
EaseInOut: Slow start and end       __/--
```
