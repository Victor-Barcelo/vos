/*
 * SDL_endian.h - VOS minimal SDL2 shim
 * Byte order detection and swapping macros
 *
 * VOS runs on x86 which is little-endian.
 */

#ifndef SDL_endian_h_
#define SDL_endian_h_

#include "SDL_stdinc.h"

/* Byte order constants */
#define SDL_LIL_ENDIAN  1234
#define SDL_BIG_ENDIAN  4321

/* VOS/x86 is little-endian */
#define SDL_BYTEORDER   SDL_LIL_ENDIAN

/*
 * Byte swapping macros
 * These swap between native byte order and big/little endian.
 */

/* Swap 16-bit value */
#define SDL_Swap16(x) \
    ((Uint16)(((Uint16)(x) >> 8) | ((Uint16)(x) << 8)))

/* Swap 32-bit value */
#define SDL_Swap32(x) \
    ((Uint32)(((Uint32)(x) >> 24) | \
              (((Uint32)(x) >> 8) & 0x0000FF00) | \
              (((Uint32)(x) << 8) & 0x00FF0000) | \
              ((Uint32)(x) << 24)))

/* Swap 64-bit value */
#define SDL_Swap64(x) \
    ((Uint64)(((Uint64)SDL_Swap32((Uint32)(x)) << 32) | \
              SDL_Swap32((Uint32)((x) >> 32))))

/* Float swap (swap as 32-bit) */
static __inline__ float SDL_SwapFloat(float x)
{
    union {
        float f;
        Uint32 ui32;
    } swapper;
    swapper.f = x;
    swapper.ui32 = SDL_Swap32(swapper.ui32);
    return swapper.f;
}

/*
 * On little-endian systems (like x86):
 * - SwapLE* is a no-op
 * - SwapBE* actually swaps
 */
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define SDL_SwapLE16(x) (x)
#define SDL_SwapLE32(x) (x)
#define SDL_SwapLE64(x) (x)
#define SDL_SwapFloatLE(x) (x)
#define SDL_SwapBE16(x) SDL_Swap16(x)
#define SDL_SwapBE32(x) SDL_Swap32(x)
#define SDL_SwapBE64(x) SDL_Swap64(x)
#define SDL_SwapFloatBE(x) SDL_SwapFloat(x)
#else
#define SDL_SwapLE16(x) SDL_Swap16(x)
#define SDL_SwapLE32(x) SDL_Swap32(x)
#define SDL_SwapLE64(x) SDL_Swap64(x)
#define SDL_SwapFloatLE(x) SDL_SwapFloat(x)
#define SDL_SwapBE16(x) (x)
#define SDL_SwapBE32(x) (x)
#define SDL_SwapBE64(x) (x)
#define SDL_SwapFloatBE(x) (x)
#endif

#endif /* SDL_endian_h_ */
