# stb_sprintf.h

Single-file public domain sprintf replacement library.

## Source

- **Repository**: https://github.com/nothings/stb
- **File**: stb_sprintf.h
- **Version**: 1.10
- **Author**: Jeff Roberts (RAD Game Tools), Sean Barrett

## License

Dual-licensed (choose one):
- **MIT License** - Copyright (c) 2017 Sean Barrett
- **Public Domain** (Unlicense) - www.unlicense.org

## Description

stb_sprintf is a full sprintf replacement that supports everything the C runtime sprintfs support, including float/double, 64-bit integers, hex floats, field parameters (%*.*d stuff), and length read-backs. It provides consistent behavior across platforms and is significantly faster than standard library implementations.

## Features

- **High Performance**: 4x to 35x faster than MSVC/GCC sprintf implementations
- **Small Code Size**: ~8KB with float support, ~4KB without (STB_SPRINTF_NOFLOAT)
- **Cross-Platform Consistency**: Unified format strings for MSVC and GCC
- **Round-Trip Float Conversion**: Bit-exact double recovery via atof
- **Extended Format Specifiers**:
  - Thousands separators with `'` (single quote): `%'d` prints 12,345
  - Metric suffixes with `$`: `%$d` converts to k/M/G/T notation
  - Binary integers with `%b`
- **64-bit Integer Support**: Both MSVC (%I64d) and GCC (%lld) styles
- **Callback-Based Output**: Custom buffer handling for streaming output
- **Always Null-Terminated**: stbsp_snprintf guarantees termination (unlike standard snprintf)

## API Reference

### Main Functions

```c
// Standard sprintf replacement
int stbsp_sprintf(char *buf, char const *fmt, ...);

// Length-limited sprintf (always null-terminates)
int stbsp_snprintf(char *buf, int count, char const *fmt, ...);

// va_list versions
int stbsp_vsprintf(char *buf, char const *fmt, va_list va);
int stbsp_vsnprintf(char *buf, int count, char const *fmt, va_list va);

// Callback-based output for custom buffer handling
int stbsp_vsprintfcb(STBSP_SPRINTFCB *callback, void *user,
                     char *buf, char const *fmt, va_list va);

// Set decimal and thousands separators
void stbsp_set_separators(char comma, char period);
```

### Callback Type

```c
typedef char *STBSP_SPRINTFCB(const char *buf, void *user, int len);
```

### Configuration Macros

| Macro | Description |
|-------|-------------|
| `STB_SPRINTF_IMPLEMENTATION` | Include implementation in this file |
| `STB_SPRINTF_STATIC` | Make all functions static |
| `STB_SPRINTF_NOFLOAT` | Disable float support (saves 4KB) |
| `STB_SPRINTF_MIN` | Minimum buffer size for callbacks (default: 512) |
| `STB_SPRINTF_DECORATE(name)` | Rename functions (default: `stbsp_##name`) |

### Supported Format Specifiers

**Types**: `s c u i d B b o X x p A a G g E e f n`

**Length Modifiers**: `hh h ll j z t I64 I32 I`

**Extensions**:
- `'` - Insert thousands separators
- `$` - Convert to metric suffix (k, M, G, T)
- `$$` - Convert to binary metric suffix (Ki, Mi, Gi, Ti)
- `$$$` - JEDEC style (K, M, G, T)
- `_` - Remove space before metric suffix

## Usage Example

```c
// In ONE source file:
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

// Basic usage
char buf[256];
stbsp_sprintf(buf, "Hello %s, you have %d messages", "User", 42);

// With thousands separators
stbsp_sprintf(buf, "Population: %'d", 1234567);  // "Population: 1,234,567"

// With metric suffixes
stbsp_sprintf(buf, "Size: %$d bytes", 2536000);  // "Size: 2.53 M bytes"

// Safe snprintf (always null-terminated)
stbsp_snprintf(buf, sizeof(buf), "Long string: %s", very_long_string);
```

## Performance

Benchmarks vs MSVC 2008 (32-bit/64-bit):

| Format | Speed Improvement |
|--------|-------------------|
| `%d` (all 32-bit ints) | 4.8x / 4.0x |
| `%f` (e-10 to e+10) | 7.3x / 6.0x |
| `%e` (e-10 to e+10) | 8.1x / 6.0x |
| `%g` (e-10 to e+10) | 10.0x / 7.1x |
| 512 char string | 35.0x / 32.5x |

## VOS/TCC Compatibility Notes

1. **Standard Headers Required**: Uses `<stdarg.h>` and `<stddef.h>`
2. **Integer Types**: Defines internal types for portability:
   - `stbsp__uint32`, `stbsp__int32`, `stbsp__uint64`, `stbsp__int64`
3. **No External Dependencies**: No CRT functions except va_args macros
4. **TCC Compatible**: No compiler-specific intrinsics required
5. **Memory Safe**: No dynamic allocation
6. **Pointer Size Detection**: Auto-detects 32/64-bit based on platform defines

### TCC-Specific Considerations

```c
// For TCC, ensure standard integer widths are available
// TCC typically supports long long for 64-bit integers
#define stbsp__uint64 unsigned long long
#define stbsp__int64 signed long long
```

## Dependencies

- `<stdarg.h>` - for va_arg(), va_list()
- `<stddef.h>` - for size_t, ptrdiff_t
