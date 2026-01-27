# stb_ds.h - Dynamic Arrays and Hash Tables

## Overview

**stb_ds.h** is a single-header library providing easy-to-use dynamic arrays and hash tables for C (also works in C++). It offers type-safe container data structures with a clean, macro-based API.

## Source

- **Original Repository:** https://github.com/nothings/stb
- **File:** stb_ds.h
- **Version:** 0.67
- **Author:** Sean Barrett (2019)

## License

This software is dual-licensed:
- **Public Domain** (www.unlicense.org)
- **MIT License**

Choose whichever you prefer.

## Features

- Type-safe dynamic arrays
- Type-safe hash maps (with any key type)
- String hash maps with automatic string management
- No memory allocations required by user (automatic reallocation)
- O(1) amortized operations
- Thread-safe read operations available
- Custom memory allocator support
- SipHash-based hashing for security
- String arena allocator for efficient string storage

## API Reference

### Compile-Time Options

| Macro | Description |
|-------|-------------|
| `STB_DS_IMPLEMENTATION` | Define in ONE file to include implementation |
| `STBDS_NO_SHORT_NAMES` | Use only `stbds_` prefixed names |
| `STBDS_SIPHASH_2_4` | Force specification-compliant SipHash-2-4 |
| `STBDS_REALLOC(c,p,s)` | Custom realloc function |
| `STBDS_FREE(c,p)` | Custom free function |
| `STBDS_UNIT_TESTS` | Enable unit test function |

### Dynamic Array Functions

All dynamic arrays are declared as: `T* arr = NULL;`

| Function | Signature | Description |
|----------|-----------|-------------|
| `arrfree` | `void arrfree(T*)` | Free the array |
| `arrlen` | `ptrdiff_t arrlen(T*)` | Get number of elements |
| `arrlenu` | `size_t arrlenu(T*)` | Get number of elements (unsigned) |
| `arrput` | `T arrput(T* a, T b)` | Append item to end, returns b |
| `arrpush` | `T arrpush(T* a, T b)` | Synonym for arrput |
| `arrpop` | `T arrpop(T* a)` | Remove and return last element |
| `arrins` | `T arrins(T* a, int p, T b)` | Insert item at position p |
| `arrinsn` | `void arrinsn(T* a, int p, int n)` | Insert n uninitialized items at p |
| `arrdel` | `void arrdel(T* a, int p)` | Delete element at position p |
| `arrdeln` | `void arrdeln(T* a, int p, int n)` | Delete n elements starting at p |
| `arrdelswap` | `void arrdelswap(T* a, int p)` | Delete at p using swap (O(1)) |
| `arrsetlen` | `void arrsetlen(T* a, int n)` | Set array length to n |
| `arrsetcap` | `size_t arrsetcap(T* a, int n)` | Set capacity to at least n |
| `arrcap` | `size_t arrcap(T* a)` | Get current capacity |
| `arraddnptr` | `T* arraddnptr(T* a, int n)` | Add n items, return pointer to first |
| `arraddnindex` | `size_t arraddnindex(T* a, int n)` | Add n items, return index of first |
| `arrlast` | `T arrlast(T* a)` | Get last element |

### Hash Map Functions

Hash maps use structs with `key` and `value` fields: `struct { TK key; TV value; } *map = NULL;`

| Function | Signature | Description |
|----------|-----------|-------------|
| `hmfree` | `void hmfree(T*)` | Free the hash map |
| `hmlen` | `ptrdiff_t hmlen(T*)` | Get number of elements |
| `hmlenu` | `size_t hmlenu(T*)` | Get number of elements (unsigned) |
| `hmput` | `TV hmput(T*, TK key, TV value)` | Insert or update key-value pair |
| `hmputs` | `T hmputs(T*, T item)` | Insert or update struct |
| `hmget` | `TV hmget(T*, TK key)` | Get value for key |
| `hmgets` | `T hmgets(T*, TK key)` | Get struct for key |
| `hmgetp` | `T* hmgetp(T*, TK key)` | Get pointer to struct for key |
| `hmgetp_null` | `T* hmgetp_null(T*, TK key)` | Get pointer or NULL if not found |
| `hmgeti` | `ptrdiff_t hmgeti(T*, TK key)` | Get index of key, or -1 |
| `hmdel` | `int hmdel(T*, TK key)` | Delete key, returns 1 if found |
| `hmdefault` | `TV hmdefault(T*, TV value)` | Set default value for missing keys |
| `hmdefaults` | `T hmdefaults(T*, T item)` | Set default struct for missing keys |

### String Hash Map Functions

String hash maps use `char*` as keys: `struct { char* key; TV value; } *map = NULL;`

| Function | Signature | Description |
|----------|-----------|-------------|
| `shfree` | `void shfree(T*)` | Free the string hash map |
| `shlen` | `ptrdiff_t shlen(T*)` | Get number of elements |
| `shput` | `TV shput(T*, char* key, TV value)` | Insert or update |
| `shputs` | `T shputs(T*, T item)` | Insert or update struct |
| `shget` | `TV shget(T*, char* key)` | Get value for key |
| `shgets` | `T shgets(T*, char* key)` | Get struct for key |
| `shgetp` | `T* shgetp(T*, char* key)` | Get pointer to struct |
| `shgetp_null` | `T* shgetp_null(T*, char* key)` | Get pointer or NULL |
| `shgeti` | `ptrdiff_t shgeti(T*, char* key)` | Get index or -1 |
| `shdel` | `int shdel(T*, char* key)` | Delete key |
| `shdefault` | `TV shdefault(T*, TV value)` | Set default value |
| `sh_new_arena` | `void sh_new_arena(T*)` | Use arena for string keys |
| `sh_new_strdup` | `void sh_new_strdup(T*)` | Duplicate string keys |

### Utility Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `stbds_rand_seed` | `void stbds_rand_seed(size_t seed)` | Seed for security |
| `stbds_hash_string` | `size_t stbds_hash_string(char* str, size_t seed)` | Hash a string |
| `stbds_hash_bytes` | `size_t stbds_hash_bytes(void* p, size_t len, size_t seed)` | Hash bytes |

## Usage Examples

### Dynamic Array

```c
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// Declare and use a dynamic array of integers
int *numbers = NULL;

arrput(numbers, 10);
arrput(numbers, 20);
arrput(numbers, 30);

for (int i = 0; i < arrlen(numbers); i++) {
    printf("%d\n", numbers[i]);
}

arrfree(numbers);
```

### Hash Map

```c
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// Define a struct for the hash map
struct { int key; float value; } *scores = NULL;

hmput(scores, 42, 3.14f);
hmput(scores, 100, 2.71f);

float val = hmget(scores, 42);  // Returns 3.14
int idx = hmgeti(scores, 999);  // Returns -1 (not found)

hmfree(scores);
```

### String Hash Map

```c
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

struct { char *key; int value; } *config = NULL;

sh_new_strdup(config);  // Duplicate strings automatically

shput(config, "width", 800);
shput(config, "height", 600);

int w = shget(config, "width");  // Returns 800

shfree(config);
```

## VOS/TCC Compatibility Notes

### Compatible Features
- All core dynamic array operations work with TCC
- Hash map operations are fully functional
- String hash maps work correctly

### Potential Issues
- TCC may not support all C99/C11 features used in some macro expansions
- For best compatibility, ensure you're using C99 mode or later
- On some platforms, you may need to seed with `stbds_rand_seed(time(NULL))` for security

### Recommended Configuration for VOS

```c
// For VOS/TCC compatibility
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// Initialize with a seed for security
void init_stb_ds(void) {
    stbds_rand_seed((size_t)time(NULL));
}
```

### Memory Considerations
- Arrays and hash maps grow automatically via realloc
- For embedded/constrained environments, consider pre-allocating with `arrsetcap`
- The library uses stdlib's realloc/free by default; override with custom allocators if needed
