/*
 * SDL_render.h - SDL2 2D rendering API for VOS
 *
 * Provides hardware-accelerated 2D rendering (software fallback for VOS).
 * Uses VOS syscalls: sys_gfx_blit_rgba(), sys_gfx_flip(), sys_gfx_clear()
 */

#ifndef SDL_RENDER_H
#define SDL_RENDER_H

#include "SDL_stdinc.h"
#include "SDL_rect.h"
#include "SDL_video.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Renderer flags */
typedef enum {
    SDL_RENDERER_SOFTWARE       = 0x00000001,   /* Software fallback */
    SDL_RENDERER_ACCELERATED    = 0x00000002,   /* Hardware accelerated */
    SDL_RENDERER_PRESENTVSYNC   = 0x00000004,   /* VSync enabled */
    SDL_RENDERER_TARGETTEXTURE  = 0x00000008    /* Supports render targets */
} SDL_RendererFlags;

/* Texture access modes */
typedef enum {
    SDL_TEXTUREACCESS_STATIC,       /* Changes rarely, not lockable */
    SDL_TEXTUREACCESS_STREAMING,    /* Changes frequently, lockable */
    SDL_TEXTUREACCESS_TARGET        /* Can be used as a render target */
} SDL_TextureAccess;

/* Texture modulate modes */
typedef enum {
    SDL_TEXTUREMODULATE_NONE  = 0x00000000,  /* No modulation */
    SDL_TEXTUREMODULATE_COLOR = 0x00000001,  /* srcC = srcC * color */
    SDL_TEXTUREMODULATE_ALPHA = 0x00000002   /* srcA = srcA * alpha */
} SDL_TextureModulate;

/* Flip constants for renderCopyEx */
typedef enum {
    SDL_FLIP_NONE       = 0x00000000,   /* Do not flip */
    SDL_FLIP_HORIZONTAL = 0x00000001,   /* Flip horizontally */
    SDL_FLIP_VERTICAL   = 0x00000002    /* Flip vertically */
} SDL_RendererFlip;

/* Blend modes */
typedef enum {
    SDL_BLENDMODE_NONE  = 0x00000000,   /* No blending */
    SDL_BLENDMODE_BLEND = 0x00000001,   /* Alpha blending */
    SDL_BLENDMODE_ADD   = 0x00000002,   /* Additive blending */
    SDL_BLENDMODE_MOD   = 0x00000004,   /* Color modulate */
    SDL_BLENDMODE_MUL   = 0x00000008    /* Color multiply */
} SDL_BlendMode;

/* Renderer info structure */
typedef struct SDL_RendererInfo {
    const char *name;               /* Name of the renderer */
    Uint32 flags;                   /* Supported SDL_RendererFlags */
    Uint32 num_texture_formats;     /* Number of texture formats */
    Uint32 texture_formats[16];     /* Available texture formats */
    int max_texture_width;          /* Maximum texture width */
    int max_texture_height;         /* Maximum texture height */
} SDL_RendererInfo;

/* Forward declarations */
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;

/* ========== Renderer Functions ========== */

/**
 * Get the number of 2D rendering drivers available.
 */
int SDL_GetNumRenderDrivers(void);

/**
 * Get info about a specific 2D rendering driver.
 */
int SDL_GetRenderDriverInfo(int index, SDL_RendererInfo *info);

/**
 * Create a 2D rendering context for a window.
 *
 * @param window The window where rendering is displayed
 * @param index The index of the rendering driver (-1 for first available)
 * @param flags SDL_RendererFlags
 * @return A valid rendering context or NULL on error
 */
SDL_Renderer* SDL_CreateRenderer(SDL_Window *window, int index, Uint32 flags);

/**
 * Create a 2D software rendering context for a surface.
 */
SDL_Renderer* SDL_CreateSoftwareRenderer(SDL_Surface *surface);

/**
 * Get the renderer associated with a window.
 */
SDL_Renderer* SDL_GetRenderer(SDL_Window *window);

/**
 * Get information about a rendering context.
 */
int SDL_GetRendererInfo(SDL_Renderer *renderer, SDL_RendererInfo *info);

/**
 * Get the output size in pixels of a rendering context.
 */
int SDL_GetRendererOutputSize(SDL_Renderer *renderer, int *w, int *h);

/**
 * Destroy the rendering context and free associated textures.
 */
void SDL_DestroyRenderer(SDL_Renderer *renderer);

/* ========== Texture Functions ========== */

/**
 * Create a texture for a rendering context.
 *
 * @param renderer The rendering context
 * @param format SDL_PixelFormatEnum value
 * @param access SDL_TextureAccess value
 * @param w Width of the texture
 * @param h Height of the texture
 * @return The created texture or NULL on error
 */
SDL_Texture* SDL_CreateTexture(SDL_Renderer *renderer, Uint32 format,
                                int access, int w, int h);

/**
 * Create a texture from an existing surface.
 */
SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface);

/**
 * Query the attributes of a texture.
 */
int SDL_QueryTexture(SDL_Texture *texture, Uint32 *format, int *access, int *w, int *h);

/**
 * Set an additional color value multiplied into render copy operations.
 */
int SDL_SetTextureColorMod(SDL_Texture *texture, Uint8 r, Uint8 g, Uint8 b);

/**
 * Get the additional color value multiplied into render copy operations.
 */
int SDL_GetTextureColorMod(SDL_Texture *texture, Uint8 *r, Uint8 *g, Uint8 *b);

/**
 * Set an additional alpha value multiplied into render copy operations.
 */
int SDL_SetTextureAlphaMod(SDL_Texture *texture, Uint8 alpha);

/**
 * Get the additional alpha value multiplied into render copy operations.
 */
int SDL_GetTextureAlphaMod(SDL_Texture *texture, Uint8 *alpha);

/**
 * Set the blend mode for a texture.
 */
int SDL_SetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode blendMode);

/**
 * Get the blend mode for a texture.
 */
int SDL_GetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode *blendMode);

/**
 * Update the given texture rectangle with new pixel data.
 *
 * @param texture The texture to update
 * @param rect Area to update, or NULL for entire texture
 * @param pixels Raw pixel data in the format of the texture
 * @param pitch Number of bytes in a row of pixel data
 * @return 0 on success or a negative error code
 */
int SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect,
                      const void *pixels, int pitch);

/**
 * Update a rectangle within a planar YV12 or IYUV texture.
 */
int SDL_UpdateYUVTexture(SDL_Texture *texture, const SDL_Rect *rect,
                          const Uint8 *Yplane, int Ypitch,
                          const Uint8 *Uplane, int Upitch,
                          const Uint8 *Vplane, int Vpitch);

/**
 * Lock a portion of the texture for write-only pixel access.
 */
int SDL_LockTexture(SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch);

/**
 * Unlock a texture, uploading the changes to video memory.
 */
void SDL_UnlockTexture(SDL_Texture *texture);

/**
 * Destroy the specified texture.
 */
void SDL_DestroyTexture(SDL_Texture *texture);

/* ========== Render Target Functions ========== */

/**
 * Set a texture as the current rendering target.
 */
int SDL_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);

/**
 * Get the current render target.
 */
SDL_Texture* SDL_GetRenderTarget(SDL_Renderer *renderer);

/* ========== Drawing Functions ========== */

/**
 * Set the color used for drawing operations.
 */
int SDL_SetRenderDrawColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/**
 * Get the color used for drawing operations.
 */
int SDL_GetRenderDrawColor(SDL_Renderer *renderer, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);

/**
 * Set the blend mode used for drawing operations.
 */
int SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode);

/**
 * Get the blend mode used for drawing operations.
 */
int SDL_GetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode *blendMode);

/**
 * Clear the current rendering target with the drawing color.
 */
int SDL_RenderClear(SDL_Renderer *renderer);

/**
 * Draw a point on the current rendering target.
 */
int SDL_RenderDrawPoint(SDL_Renderer *renderer, int x, int y);

/**
 * Draw multiple points on the current rendering target.
 */
int SDL_RenderDrawPoints(SDL_Renderer *renderer, const SDL_Point *points, int count);

/**
 * Draw a line on the current rendering target.
 */
int SDL_RenderDrawLine(SDL_Renderer *renderer, int x1, int y1, int x2, int y2);

/**
 * Draw a series of connected lines on the current rendering target.
 */
int SDL_RenderDrawLines(SDL_Renderer *renderer, const SDL_Point *points, int count);

/**
 * Draw a rectangle on the current rendering target.
 */
int SDL_RenderDrawRect(SDL_Renderer *renderer, const SDL_Rect *rect);

/**
 * Draw some number of rectangles on the current rendering target.
 */
int SDL_RenderDrawRects(SDL_Renderer *renderer, const SDL_Rect *rects, int count);

/**
 * Fill a rectangle on the current rendering target.
 */
int SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_Rect *rect);

/**
 * Fill some number of rectangles on the current rendering target.
 */
int SDL_RenderFillRects(SDL_Renderer *renderer, const SDL_Rect *rects, int count);

/**
 * Copy a portion of the texture to the current rendering target.
 *
 * @param renderer The rendering context
 * @param texture The source texture
 * @param srcrect The source rectangle, or NULL for entire texture
 * @param dstrect The destination rectangle, or NULL for entire target
 * @return 0 on success or a negative error code
 */
int SDL_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture,
                   const SDL_Rect *srcrect, const SDL_Rect *dstrect);

/**
 * Copy a portion of the texture to the current rendering target, with rotation/flipping.
 */
int SDL_RenderCopyEx(SDL_Renderer *renderer, SDL_Texture *texture,
                     const SDL_Rect *srcrect, const SDL_Rect *dstrect,
                     const double angle, const SDL_Point *center,
                     const SDL_RendererFlip flip);

/**
 * Read pixels from the current rendering target.
 */
int SDL_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
                          Uint32 format, void *pixels, int pitch);

/**
 * Update the screen with any rendering performed since the previous call.
 *
 * This uses VOS sys_gfx_blit_rgba() and sys_gfx_flip().
 */
void SDL_RenderPresent(SDL_Renderer *renderer);

/**
 * Set the drawing scale for rendering.
 */
int SDL_RenderSetScale(SDL_Renderer *renderer, float scaleX, float scaleY);

/**
 * Get the drawing scale for the current target.
 */
void SDL_RenderGetScale(SDL_Renderer *renderer, float *scaleX, float *scaleY);

/**
 * Set the drawing area for rendering.
 */
int SDL_RenderSetViewport(SDL_Renderer *renderer, const SDL_Rect *rect);

/**
 * Get the drawing area for the current target.
 */
void SDL_RenderGetViewport(SDL_Renderer *renderer, SDL_Rect *rect);

/**
 * Set the clip rectangle for rendering.
 */
int SDL_RenderSetClipRect(SDL_Renderer *renderer, const SDL_Rect *rect);

/**
 * Get the clip rectangle for the current target.
 */
void SDL_RenderGetClipRect(SDL_Renderer *renderer, SDL_Rect *rect);

/**
 * Check if clipping is enabled.
 */
SDL_bool SDL_RenderIsClipEnabled(SDL_Renderer *renderer);

/**
 * Set integer scale mode.
 */
int SDL_RenderSetIntegerScale(SDL_Renderer *renderer, SDL_bool enable);

/**
 * Get integer scale mode.
 */
SDL_bool SDL_RenderGetIntegerScale(SDL_Renderer *renderer);

/**
 * Set device independent resolution for rendering.
 */
int SDL_RenderSetLogicalSize(SDL_Renderer *renderer, int w, int h);

/**
 * Get device independent resolution for rendering.
 */
void SDL_RenderGetLogicalSize(SDL_Renderer *renderer, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif /* SDL_RENDER_H */
