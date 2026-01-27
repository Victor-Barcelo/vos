# stb_leakcheck.h

Quick and dirty malloc memory leak checking library.

## Source

- **Repository**: https://github.com/nothings/stb
- **File**: stb_leakcheck.h
- **Version**: 0.6
- **Author**: Sean Barrett

## License

Dual-licensed (choose one):
- **MIT License** - Copyright (c) 2017 Sean Barrett
- **Public Domain** (Unlicense) - www.unlicense.org

## Description

stb_leakcheck is a lightweight memory leak detection utility that intercepts standard memory allocation functions (malloc, free, realloc) to track heap usage. It maintains a linked list of allocation metadata including source file, line number, and allocation size, then reports any unfreed memory blocks.

## Features

- **Automatic Tracking**: Transparent replacement of malloc/free/realloc via macros
- **Source Location**: Records file and line number for each allocation
- **Minimal Overhead**: Only adds one struct per allocation
- **Simple Integration**: Single header include activates functionality
- **Configurable Output**: Direct leak reports to custom output stream
- **Debug Mode**: Optional display of all allocations (freed and leaked)
- **Realloc Tracking**: Optionally preserve original allocation site through reallocs

## API Reference

### Functions

```c
// Allocate memory with source tracking
void *stb_leakcheck_malloc(size_t sz, const char *file, int line);

// Reallocate with tracking
void *stb_leakcheck_realloc(void *ptr, size_t sz, const char *file, int line);

// Free tracked memory
void stb_leakcheck_free(void *ptr);

// Print all leaked (unfreed) allocations
void stb_leakcheck_dumpmem(void);
```

### Macros (Automatic Replacement)

```c
// These replace standard functions automatically:
#define malloc(sz)    stb_leakcheck_malloc(sz, __FILE__, __LINE__)
#define free(p)       stb_leakcheck_free(p)
#define realloc(p,sz) stb_leakcheck_realloc(p,sz, __FILE__, __LINE__)
```

### Configuration Macros

| Macro | Description |
|-------|-------------|
| `STB_LEAKCHECK_IMPLEMENTATION` | Include implementation |
| `STB_LEAKCHECK_OUTPUT_PIPE` | Output stream (default: stdout) |
| `STB_LEAKCHECK_SHOWALL` | Also show freed allocations in dump |
| `STB_LEAKCHECK_REALLOC_PRESERVE_MALLOC_FILELINE` | Keep original file/line on realloc |

### Internal Structure

```c
struct malloc_info {
    const char *file;              // Source file of allocation
    int line;                      // Line number
    size_t size;                   // Allocation size
    stb_leakcheck_malloc_info *next, *prev;  // Linked list
};
```

## Usage Example

### Basic Usage

```c
// In ONE source file (typically main.c), add BEFORE any malloc calls:
#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb_leakcheck.h"

// In other files, just include:
#include "stb_leakcheck.h"

int main() {
    // Normal malloc/free - automatically tracked
    char *str = malloc(100);
    int *arr = malloc(sizeof(int) * 50);

    free(str);  // This is properly freed
    // arr is leaked!

    // At program end, dump leaked memory:
    stb_leakcheck_dumpmem();
    // Output: LEAKED: main.c (   8): 200 bytes at 0x12345678

    return 0;
}
```

### Custom Output Stream

```c
#define STB_LEAKCHECK_OUTPUT_PIPE stderr
#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb_leakcheck.h"
```

### Show All Allocations (Debug Mode)

```c
#define STB_LEAKCHECK_SHOWALL
#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb_leakcheck.h"

// stb_leakcheck_dumpmem() will show:
// LEAKED: file.c (10): 100 bytes at 0x...
// FREED : file.c (15): 50 bytes at 0x...
```

### Preserve Original Allocation Site

```c
// Normally, realloc updates file/line to the realloc call site
// This option preserves the original malloc location:
#define STB_LEAKCHECK_REALLOC_PRESERVE_MALLOC_FILELINE
#define STB_LEAKCHECK_IMPLEMENTATION
#include "stb_leakcheck.h"
```

## Output Format

Leak reports follow this format:
```
LEAKED: filename.c (  42): 1024 bytes at 0x7fff5fbff8c0
```

With `STB_LEAKCHECK_SHOWALL`:
```
LEAKED: filename.c (  42): 1024 bytes at 0x7fff5fbff8c0
FREED : filename.c (  50): 256 bytes at 0x7fff5fbff9d0
```

## Important Notes

1. **Include Order**: Must be included AFTER `<stdlib.h>` to properly override macros
2. **All Files**: Include in ALL source files that use malloc/free for complete tracking
3. **Overhead**: Each allocation adds sizeof(malloc_info) bytes (~32 bytes on 64-bit)
4. **Thread Safety**: NOT thread-safe - use mutex if needed in multi-threaded code
5. **Performance**: Adds linked list operations to each alloc/free

## VOS/TCC Compatibility Notes

1. **Standard Headers Required**:
   - `<assert.h>` - for assert()
   - `<string.h>` - for memcpy()
   - `<stdlib.h>` - for actual malloc/free
   - `<stdio.h>` - for fprintf()
   - `<stddef.h>` - for size_t, ptrdiff_t

2. **Preprocessor Features**: Uses `__FILE__` and `__LINE__` macros

3. **TCC Compatible**: All features work with TCC

4. **No Platform-Specific Code**: Pure portable C

### Compiler-Specific Size Format

The library handles `%zd` format specifier compatibility:
- Modern compilers: Uses `%zd` for size_t
- Old MSVC (<2015): Falls back to `%lld` or `%d`
- MinGW: Uses `__mingw_fprintf` for proper format handling

### Disabling for Release Builds

```c
#ifdef DEBUG
    #define STB_LEAKCHECK_IMPLEMENTATION
    #include "stb_leakcheck.h"
#else
    // In release, just use standard functions
    #include <stdlib.h>
#endif
```

## Limitations

- Single-threaded use only (no synchronization)
- Does not detect buffer overruns
- Does not detect use-after-free
- Does not detect double-free (will corrupt list)
- Overhead per allocation: ~32 bytes + linked list operations

## Dependencies

- `<assert.h>` - for assert()
- `<string.h>` - for memcpy()
- `<stdlib.h>` - for malloc, realloc, free
- `<stdio.h>` - for fprintf, stdout
- `<stddef.h>` - for size_t, ptrdiff_t
