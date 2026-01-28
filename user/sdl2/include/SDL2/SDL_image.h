/*
 * SDL_image.h - VOS minimal SDL_image shim
 *
 * Provides PNG/BMP/etc image loading using stb_image.
 * Only implements the subset needed by klystrack.
 */

#ifndef SDL_IMAGE_H
#define SDL_IMAGE_H

#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "SDL_rwops.h"

#ifdef __cplusplus
extern "C" {
#endif

/* IMG_Init flags */
#define IMG_INIT_JPG    0x00000001
#define IMG_INIT_PNG    0x00000002
#define IMG_INIT_TIF    0x00000004
#define IMG_INIT_WEBP   0x00000008

/**
 * Initialize SDL_image
 *
 * flags: IMG_INIT_* flags for formats to initialize
 * Returns: Flags that were successfully initialized
 *
 * Note: VOS implementation doesn't require initialization,
 * this always returns the requested flags.
 */
int IMG_Init(int flags);

/**
 * Cleanup SDL_image
 *
 * Note: VOS implementation is a no-op.
 */
void IMG_Quit(void);

/**
 * Load an image from an SDL_RWops
 *
 * src: SDL_RWops to read from
 * freesrc: If non-zero, close the RWops after loading
 *
 * Returns: SDL_Surface with image data, or NULL on error
 *
 * Supports: PNG, BMP, TGA, JPG, GIF (via stb_image)
 */
SDL_Surface* IMG_Load_RW(SDL_RWops *src, int freesrc);

/**
 * Load an image from a file path
 *
 * file: Path to the image file
 *
 * Returns: SDL_Surface with image data, or NULL on error
 */
SDL_Surface* IMG_Load(const char *file);

/**
 * Check if an RWops contains a specific format
 * (Used for format detection)
 */
int IMG_isPNG(SDL_RWops *src);
int IMG_isBMP(SDL_RWops *src);
int IMG_isJPG(SDL_RWops *src);
int IMG_isGIF(SDL_RWops *src);
int IMG_isTIF(SDL_RWops *src);

/**
 * Load specific format from RWops
 */
SDL_Surface* IMG_LoadPNG_RW(SDL_RWops *src);
SDL_Surface* IMG_LoadBMP_RW(SDL_RWops *src);
SDL_Surface* IMG_LoadJPG_RW(SDL_RWops *src);
SDL_Surface* IMG_LoadGIF_RW(SDL_RWops *src);
SDL_Surface* IMG_LoadTGA_RW(SDL_RWops *src);

#ifdef __cplusplus
}
#endif

#endif /* SDL_IMAGE_H */
