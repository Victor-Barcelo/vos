/*
 * SDL_stdinc.h - VOS minimal SDL2 shim
 * Basic types and definitions compatible with SDL2 API
 */

#ifndef SDL_stdinc_h_
#define SDL_stdinc_h_

#include <stdint.h>
#include <stddef.h>

/* Boolean type */
typedef enum {
    SDL_FALSE = 0,
    SDL_TRUE = 1
} SDL_bool;

/* Basic integer types */
typedef int8_t      Sint8;
typedef uint8_t     Uint8;
typedef int16_t     Sint16;
typedef uint16_t    Uint16;
typedef int32_t     Sint32;
typedef uint32_t    Uint32;
typedef int64_t     Sint64;
typedef uint64_t    Uint64;

/* Make sure the types are the expected size */
#define SDL_COMPILE_TIME_ASSERT(name, x) \
    typedef int SDL_compile_time_assert_##name[(x) ? 1 : -1]

SDL_COMPILE_TIME_ASSERT(sint8,  sizeof(Sint8)  == 1);
SDL_COMPILE_TIME_ASSERT(uint8,  sizeof(Uint8)  == 1);
SDL_COMPILE_TIME_ASSERT(sint16, sizeof(Sint16) == 2);
SDL_COMPILE_TIME_ASSERT(uint16, sizeof(Uint16) == 2);
SDL_COMPILE_TIME_ASSERT(sint32, sizeof(Sint32) == 4);
SDL_COMPILE_TIME_ASSERT(uint32, sizeof(Uint32) == 4);
SDL_COMPILE_TIME_ASSERT(sint64, sizeof(Sint64) == 8);
SDL_COMPILE_TIME_ASSERT(uint64, sizeof(Uint64) == 8);

/* Macro to mark function parameters as unused */
#define SDL_UNUSED(x) (void)(x)

/* NULL definition */
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

/* Inline macro for TCC compatibility */
#ifndef SDL_INLINE
#define SDL_INLINE static inline
#endif

/* Force inline - just use inline for TCC */
#ifndef SDL_FORCE_INLINE
#define SDL_FORCE_INLINE static inline
#endif

/* Min/Max macros */
#define SDL_min(x, y) (((x) < (y)) ? (x) : (y))
#define SDL_max(x, y) (((x) > (y)) ? (x) : (y))
#define SDL_clamp(x, a, b) (((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x)))

/* Memory functions - these would be implemented or mapped to libc */
void *SDL_malloc(size_t size);
void *SDL_calloc(size_t nmemb, size_t size);
void *SDL_realloc(void *mem, size_t size);
void SDL_free(void *mem);

/* String functions */
size_t SDL_strlen(const char *str);
char *SDL_strcpy(char *dst, const char *src);
char *SDL_strncpy(char *dst, const char *src, size_t maxlen);
int SDL_strcmp(const char *str1, const char *str2);
int SDL_strncmp(const char *str1, const char *str2, size_t maxlen);

/* Memory copy/set */
void *SDL_memset(void *dst, int c, size_t len);
void *SDL_memcpy(void *dst, const void *src, size_t len);
void *SDL_memmove(void *dst, const void *src, size_t len);
int SDL_memcmp(const void *s1, const void *s2, size_t len);

/* Zero memory helper */
#define SDL_zero(x) SDL_memset(&(x), 0, sizeof((x)))
#define SDL_zerop(x) SDL_memset((x), 0, sizeof(*(x)))
#define SDL_zeroa(x) SDL_memset((x), 0, sizeof((x)))

/* Array size macro */
#define SDL_arraysize(array) (sizeof(array) / sizeof(array[0]))

/* FourCC macro for creating format codes */
#define SDL_FOURCC(A, B, C, D) \
    ((Uint32)(Uint8)(A) | ((Uint32)(Uint8)(B) << 8) | \
     ((Uint32)(Uint8)(C) << 16) | ((Uint32)(Uint8)(D) << 24))

#endif /* SDL_stdinc_h_ */
