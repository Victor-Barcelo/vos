/*
 * SDL_video.h - SDL2 video subsystem for VOS
 *
 * Minimal implementation using VOS graphics syscalls:
 * - sys_gfx_clear(), sys_gfx_pset(), sys_gfx_blit_rgba()
 * - sys_gfx_flip(), sys_gfx_double_buffer()
 * - Screen size via TIOCGWINSZ ioctl
 */

#ifndef SDL_VIDEO_H
#define SDL_VIDEO_H

#include "SDL_stdinc.h"
#include "SDL_rect.h"
#include "SDL_pixels.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Window position constants */
#define SDL_WINDOWPOS_UNDEFINED_MASK    0x1FFF0000u
#define SDL_WINDOWPOS_UNDEFINED         SDL_WINDOWPOS_UNDEFINED_MASK
#define SDL_WINDOWPOS_CENTERED_MASK     0x2FFF0000u
#define SDL_WINDOWPOS_CENTERED          SDL_WINDOWPOS_CENTERED_MASK

#define SDL_WINDOWPOS_ISUNDEFINED(X)    (((X)&0xFFFF0000) == SDL_WINDOWPOS_UNDEFINED_MASK)
#define SDL_WINDOWPOS_ISCENTERED(X)     (((X)&0xFFFF0000) == SDL_WINDOWPOS_CENTERED_MASK)

/* Window flags */
typedef enum {
    SDL_WINDOW_FULLSCREEN           = 0x00000001,
    SDL_WINDOW_OPENGL               = 0x00000002,
    SDL_WINDOW_SHOWN                = 0x00000004,
    SDL_WINDOW_HIDDEN               = 0x00000008,
    SDL_WINDOW_BORDERLESS           = 0x00000010,
    SDL_WINDOW_RESIZABLE            = 0x00000020,
    SDL_WINDOW_MINIMIZED            = 0x00000040,
    SDL_WINDOW_MAXIMIZED            = 0x00000080,
    SDL_WINDOW_MOUSE_GRABBED        = 0x00000100,
    SDL_WINDOW_INPUT_FOCUS          = 0x00000200,
    SDL_WINDOW_MOUSE_FOCUS          = 0x00000400,
    SDL_WINDOW_FULLSCREEN_DESKTOP   = (SDL_WINDOW_FULLSCREEN | 0x00001000),
    SDL_WINDOW_FOREIGN              = 0x00000800,
    SDL_WINDOW_ALLOW_HIGHDPI        = 0x00002000,
    SDL_WINDOW_MOUSE_CAPTURE        = 0x00004000,
    SDL_WINDOW_ALWAYS_ON_TOP        = 0x00008000,
    SDL_WINDOW_SKIP_TASKBAR         = 0x00010000,
    SDL_WINDOW_UTILITY              = 0x00020000,
    SDL_WINDOW_TOOLTIP              = 0x00040000,
    SDL_WINDOW_POPUP_MENU           = 0x00080000,
    SDL_WINDOW_KEYBOARD_GRABBED     = 0x00100000,
    SDL_WINDOW_VULKAN               = 0x10000000,
    SDL_WINDOW_METAL                = 0x20000000
} SDL_WindowFlags;

/* Forward declaration */
typedef struct SDL_Window SDL_Window;

/**
 * SDL Surface - a collection of pixels used for software blitting
 */
typedef struct SDL_Surface {
    Uint32 flags;               /* Read-only */
    SDL_PixelFormat *format;    /* Read-only */
    int w, h;                   /* Read-only */
    int pitch;                  /* Read-only */
    void *pixels;               /* Read-write */
    void *userdata;             /* Application-specific data */
    int locked;                 /* Read-only */
    void *lock_data;            /* Read-only */
    SDL_Rect clip_rect;         /* Read-only */
    struct SDL_BlitMap *map;    /* Private */
    int refcount;               /* Read-mostly */
} SDL_Surface;

/* Surface flags (for compatibility) */
#define SDL_SWSURFACE       0
#define SDL_PREALLOC        0x00000001
#define SDL_RLEACCEL        0x00000002
#define SDL_DONTFREE        0x00000004
#define SDL_SIMD_ALIGNED    0x00000008

/* SDL_MUSTLOCK - check if surface needs locking before access */
#define SDL_MUSTLOCK(surface) (((surface)->flags & SDL_RLEACCEL) != 0)

/* Display mode structure */
typedef struct SDL_DisplayMode {
    Uint32 format;              /* Pixel format */
    int w;                      /* Width in screen coordinates */
    int h;                      /* Height in screen coordinates */
    int refresh_rate;           /* Refresh rate (or 0 for unspecified) */
    void *driverdata;           /* Driver-specific data */
} SDL_DisplayMode;

/* ========== Video Subsystem Functions ========== */

/**
 * Get the number of video drivers compiled into SDL.
 */
int SDL_GetNumVideoDrivers(void);

/**
 * Get the name of a video driver.
 */
const char* SDL_GetVideoDriver(int index);

/**
 * Initialize the video subsystem.
 */
int SDL_VideoInit(const char *driver_name);

/**
 * Shut down the video subsystem.
 */
void SDL_VideoQuit(void);

/**
 * Get the name of the currently initialized video driver.
 */
const char* SDL_GetCurrentVideoDriver(void);

/**
 * Get the number of available video displays.
 */
int SDL_GetNumVideoDisplays(void);

/**
 * Get the name of a display.
 */
const char* SDL_GetDisplayName(int displayIndex);

/**
 * Get the desktop display mode.
 */
int SDL_GetDesktopDisplayMode(int displayIndex, SDL_DisplayMode *mode);

/**
 * Get the current display mode.
 */
int SDL_GetCurrentDisplayMode(int displayIndex, SDL_DisplayMode *mode);

/* ========== Window Functions ========== */

/**
 * Create a window with the specified position, dimensions, and flags.
 */
SDL_Window* SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags);

/**
 * Destroy a window.
 */
void SDL_DestroyWindow(SDL_Window *window);

/**
 * Get the surface associated with the window.
 *
 * A new surface will be created with the optimal format for the window.
 * This surface will be freed when the window is destroyed.
 */
SDL_Surface* SDL_GetWindowSurface(SDL_Window *window);

/**
 * Copy the window surface to the screen.
 *
 * This uses VOS sys_gfx_blit_rgba() and sys_gfx_flip().
 */
int SDL_UpdateWindowSurface(SDL_Window *window);

/**
 * Copy areas of the window surface to the screen.
 */
int SDL_UpdateWindowSurfaceRects(SDL_Window *window, const SDL_Rect *rects, int numrects);

/**
 * Get the size of the window's client area.
 */
void SDL_GetWindowSize(SDL_Window *window, int *w, int *h);

/**
 * Set the size of the window's client area.
 */
void SDL_SetWindowSize(SDL_Window *window, int w, int h);

/**
 * Get the position of a window.
 */
void SDL_GetWindowPosition(SDL_Window *window, int *x, int *y);

/**
 * Set the position of a window.
 */
void SDL_SetWindowPosition(SDL_Window *window, int x, int y);

/**
 * Get the title of a window.
 */
const char* SDL_GetWindowTitle(SDL_Window *window);

/**
 * Set the title of a window.
 */
void SDL_SetWindowTitle(SDL_Window *window, const char *title);

/**
 * Set the minimum size of a window's client area.
 */
void SDL_SetWindowMinimumSize(SDL_Window *window, int min_w, int min_h);

/**
 * Get the minimum size of a window's client area.
 */
void SDL_GetWindowMinimumSize(SDL_Window *window, int *w, int *h);

/**
 * Show a window.
 */
void SDL_ShowWindow(SDL_Window *window);

/**
 * Hide a window.
 */
void SDL_HideWindow(SDL_Window *window);

/**
 * Raise a window above other windows.
 */
void SDL_RaiseWindow(SDL_Window *window);

/**
 * Minimize a window to an iconic representation.
 */
void SDL_MinimizeWindow(SDL_Window *window);

/**
 * Maximize a window.
 */
void SDL_MaximizeWindow(SDL_Window *window);

/**
 * Restore the size and position of a minimized or maximized window.
 */
void SDL_RestoreWindow(SDL_Window *window);

/**
 * Set a window's fullscreen state.
 */
int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags);

/**
 * Get the window flags.
 */
Uint32 SDL_GetWindowFlags(SDL_Window *window);

/**
 * Get the window ID.
 */
Uint32 SDL_GetWindowID(SDL_Window *window);

/**
 * Get a window from a stored ID.
 */
SDL_Window* SDL_GetWindowFromID(Uint32 id);

/* ========== Surface Functions ========== */

/**
 * Allocate a new RGB surface.
 */
SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                   Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);

/**
 * Allocate a new RGB surface with existing pixel data.
 */
SDL_Surface* SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch,
                                       Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);

/**
 * Allocate a new RGB surface with a specific pixel format.
 */
SDL_Surface* SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth, Uint32 format);

/**
 * Allocate a new RGB surface with existing pixel data and a specific format.
 */
SDL_Surface* SDL_CreateRGBSurfaceWithFormatFrom(void *pixels, int width, int height, int depth,
                                                  int pitch, Uint32 format);

/**
 * Free an RGB surface.
 */
void SDL_FreeSurface(SDL_Surface *surface);

/**
 * Set the color key (transparent pixel) for a surface.
 */
int SDL_SetColorKey(SDL_Surface *surface, int flag, Uint32 key);

/**
 * Get the color key for a surface.
 */
int SDL_GetColorKey(SDL_Surface *surface, Uint32 *key);

/**
 * Set an additional alpha value used in blit operations.
 */
int SDL_SetSurfaceAlphaMod(SDL_Surface *surface, Uint8 alpha);

/**
 * Get the additional alpha value used in blit operations.
 */
int SDL_GetSurfaceAlphaMod(SDL_Surface *surface, Uint8 *alpha);

/**
 * Set the blend mode used for surface blit operations.
 */
int SDL_SetSurfaceBlendMode(SDL_Surface *surface, int blendMode);

/**
 * Get the blend mode used for surface blit operations.
 */
int SDL_GetSurfaceBlendMode(SDL_Surface *surface, int *blendMode);

/**
 * Set the clipping rectangle for a surface.
 */
SDL_bool SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect);

/**
 * Get the clipping rectangle for a surface.
 */
void SDL_GetClipRect(SDL_Surface *surface, SDL_Rect *rect);

/**
 * Perform a fast fill of a rectangle with a specific color.
 */
int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color);

/**
 * Perform a fast fill of rectangles with a specific color.
 */
int SDL_FillRects(SDL_Surface *dst, const SDL_Rect *rects, int count, Uint32 color);

/**
 * Perform a fast blit from the source surface to the destination surface.
 */
int SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect,
                    SDL_Surface *dst, SDL_Rect *dstrect);

/**
 * Perform a scaled blit.
 */
int SDL_BlitScaled(SDL_Surface *src, const SDL_Rect *srcrect,
                   SDL_Surface *dst, SDL_Rect *dstrect);

/**
 * Perform low-level surface blitting (SDL_UpperBlit).
 */
int SDL_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect,
                  SDL_Surface *dst, SDL_Rect *dstrect);

/**
 * Lock a surface for direct access.
 */
int SDL_LockSurface(SDL_Surface *surface);

/**
 * Unlock a previously locked surface.
 */
void SDL_UnlockSurface(SDL_Surface *surface);

/**
 * Perform a scaled stretch blit.
 */
int SDL_SoftStretch(SDL_Surface *src, const SDL_Rect *srcrect,
                    SDL_Surface *dst, const SDL_Rect *dstrect);

/**
 * Convert a surface to the same format as another surface.
 */
SDL_Surface* SDL_ConvertSurface(SDL_Surface *src, const SDL_PixelFormat *fmt, Uint32 flags);

/**
 * Copy an existing surface to a new surface of the specified format.
 */
SDL_Surface* SDL_ConvertSurfaceFormat(SDL_Surface *src, Uint32 pixel_format, Uint32 flags);

/* Forward declare SDL_RWops */
struct SDL_RWops;

/**
 * Load a BMP image from an SDL_RWops.
 *
 * src: The RWops to read from
 * freesrc: If non-zero, the RWops will be closed after reading
 * Returns a new SDL_Surface, or NULL on error.
 */
SDL_Surface* SDL_LoadBMP_RW(struct SDL_RWops *src, int freesrc);

/**
 * Load a BMP image from a file.
 */
#define SDL_LoadBMP(file)   SDL_LoadBMP_RW(SDL_RWFromFile(file, "rb"), 1)

/* Convenience macro for BlitSurface */
#define SDL_BlitSurface SDL_UpperBlit

#ifdef __cplusplus
}
#endif

#endif /* SDL_VIDEO_H */
