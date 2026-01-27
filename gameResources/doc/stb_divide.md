# stb_divide.h

Consistent signed integer division and modulus operations.

## Source

- **Repository**: https://github.com/nothings/stb
- **File**: stb_divide.h
- **Version**: 0.94
- **Author**: Sean Barrett

## License

Dual-licensed (choose one):
- **MIT License** - Copyright (c) 2017 Sean Barrett
- **Public Domain** (Unlicense) - www.unlicense.org

## Description

stb_divide provides three mathematically consistent divide/modulus pairs implemented on top of arbitrary C/C++ division, with correct handling of overflow in intermediate calculations. This addresses the problem that C's behavior for signed integer division with negative operands is implementation-defined (before C99) or may not match mathematical expectations.

## Features

- **Three Division Models**: Truncating, floor, and Euclidean
- **Overflow Safe**: Correct handling of INT_MIN edge cases
- **Platform Independent**: Works regardless of native C division behavior
- **Self-Testing**: Built-in test suite available
- **No Dependencies**: Only requires `<limits.h>` for INT_MIN

## Division Models Explained

### Truncating Division (`trunc`)
- Quotient truncates toward zero
- Remainder has same sign as dividend
- Easiest to implement in hardware
- C99 standard behavior

### Floor Division (`floor`)
- Quotient truncates toward negative infinity
- Remainder has same sign as divisor
- Divides integers into fixed-size buckets without extra-wide bucket at 0
- Python's default division behavior

### Euclidean Division (`eucl`)
- Quotient truncates toward sign(divisor) * infinity
- Remainder is always non-negative
- Guarantees fixed-size buckets AND non-negative modulus
- Mathematically cleanest definition

### Comparison Examples

| a | b | trunc div | trunc mod | floor div | floor mod | eucl div | eucl mod |
|---|---|-----------|-----------|-----------|-----------|----------|----------|
| 8 | 3 | 2 | 2 | 2 | 2 | 2 | 2 |
| -8 | 3 | -2 | -2 | -3 | 1 | -3 | 1 |
| 8 | -3 | -2 | 2 | -3 | -1 | -2 | 2 |
| -8 | -3 | 2 | -2 | 2 | -2 | 3 | 1 |

## API Reference

### Division Functions

```c
// Truncating division (quotient toward zero)
int stb_div_trunc(int value_to_be_divided, int value_to_divide_by);

// Floor division (quotient toward -infinity)
int stb_div_floor(int value_to_be_divided, int value_to_divide_by);

// Euclidean division (non-negative remainder)
int stb_div_eucl(int value_to_be_divided, int value_to_divide_by);
```

### Modulus Functions

```c
// Truncating modulus (same sign as dividend)
int stb_mod_trunc(int value_to_be_divided, int value_to_divide_by);

// Floor modulus (same sign as divisor)
int stb_mod_floor(int value_to_be_divided, int value_to_divide_by);

// Euclidean modulus (always non-negative)
int stb_mod_eucl(int value_to_be_divided, int value_to_divide_by);
```

### Configuration Macros

| Macro | Description |
|-------|-------------|
| `STB_DIVIDE_IMPLEMENTATION` | Include implementation |
| `C_INTEGER_DIVISION_TRUNCATES` | Optimize for truncating C division |
| `C_INTEGER_DIVISION_FLOORS` | Optimize for flooring C division |
| `STB_DIVIDE_TEST` | Enable test suite (generates main()) |
| `STB_DIVIDE_TEST_64` | 64-bit type for test overflow checking |

## Usage Example

```c
// In ONE source file:
#define STB_DIVIDE_IMPLEMENTATION
#include "stb_divide.h"

// Other files just include normally
#include "stb_divide.h"

// Usage
int q, r;

// For pixel coordinates (floor is best - consistent bucket sizes)
q = stb_div_floor(-5, 3);  // -2 (not -1 as in C truncation)
r = stb_mod_floor(-5, 3);  // 1  (not -2)

// For array indexing with wrap-around
int wrapped_index = stb_mod_eucl(index, array_size);  // Always 0 to size-1

// Tile-based games (which tile is point in?)
int tile_x = stb_div_floor(world_x, TILE_SIZE);
int tile_y = stb_div_floor(world_y, TILE_SIZE);
```

### Platform Optimization

```c
// If you know your platform truncates (C99+, most modern platforms):
#define C_INTEGER_DIVISION_TRUNCATES
#define STB_DIVIDE_IMPLEMENTATION
#include "stb_divide.h"
// stb_div_trunc and stb_mod_trunc become simple wrappers
```

### Running Tests

```c
// Compile with STB_DIVIDE_TEST to create test program
#define STB_DIVIDE_TEST
#define STB_DIVIDE_TEST_64 long long  // For 64-bit overflow checking
#define STB_DIVIDE_IMPLEMENTATION
#include "stb_divide.h"

// Compile and run - no output means success
// Run with any argument to see all test results
```

## Mathematical Properties

All implementations guarantee:
1. `(a/b)*b + (a%b) == a` (required by C standard)
2. If `a` and `b` have the same sign, all three divisions give the same result

Unique properties:
- **trunc**: `|a%b| < |b|`, sign(a%b) == sign(a) or a%b == 0
- **floor**: `|a%b| < |b|`, sign(a%b) == sign(b) or a%b == 0
- **eucl**: `0 <= a%b < |b|` (remainder always non-negative)

## VOS/TCC Compatibility Notes

1. **Standard Headers Required**:
   - `<limits.h>` - for INT_MIN (can be defined manually if unavailable)

2. **No Floating Point**: Pure integer arithmetic

3. **No Dynamic Allocation**: Stack-only operations

4. **C89 Compatible**: Works with any C compiler

5. **TCC Compatible**: No special features required

### Minimal INT_MIN Definition

If `<limits.h>` is unavailable:
```c
// For 32-bit int
#define INT_MIN (-2147483647 - 1)
```

### Performance Considerations

- With `C_INTEGER_DIVISION_TRUNCATES`, trunc operations are single divisions
- Without optimization flags, each operation may require 2-3 divisions
- All operations are O(1) constant time

## References

- "The Euclidean definition of the functions div and mod" - Raymond Boute (1992)
- "Division and Modulus for Computer Scientists" - Daan Leijen (2001)

## Dependencies

- `<limits.h>` - for INT_MIN (optional, can be defined manually)
