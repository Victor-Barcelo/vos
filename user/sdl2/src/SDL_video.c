/*
 * SDL2 Video Subsystem Implementation for VOS
 *
 * Maps SDL2 video/render API to VOS graphics syscalls:
 * - sys_screen_is_fb() - check if framebuffer mode is available
 * - sys_gfx_clear(bg) - clear screen with background color
 * - sys_gfx_pset(x, y, color) - plot a single pixel
 * - sys_gfx_blit_rgba(x, y, w, h, rgba_ptr) - blit RGBA image data
 * - sys_gfx_flip() - flip double buffer
 * - sys_gfx_double_buffer(enable) - enable/disable double buffering
 * - Screen size via ioctl(STDIN_FILENO, TIOCGWINSZ, &ws)
 *
 * This implementation uses a software rendering surface that gets blitted
 * to the VOS framebuffer using sys_gfx_blit_rgba().
 */

#include "SDL2/SDL_video.h"
#include "SDL2/SDL_render.h"
#include "SDL2/SDL_pixels.h"
#include "SDL2/SDL_stdinc.h"
#include "SDL2/SDL_rwops.h"
#include "syscall.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* ========== Internal Structures ========== */

/* Internal window structure */
struct SDL_Window {
    Uint32 id;
    char title[256];
    int x, y;
    int w, h;
    int min_w, min_h;           /* Minimum window size */
    Uint32 flags;
    SDL_Surface *surface;       /* Window surface for software rendering */
    struct SDL_Renderer *renderer;
};

/* Internal renderer structure */
struct SDL_Renderer {
    SDL_Window *window;
    SDL_Surface *target;        /* Current render target (window surface or texture) */
    Uint8 r, g, b, a;           /* Current draw color */
    SDL_BlendMode blend_mode;
    SDL_Rect viewport;
    SDL_Rect clip_rect;
    SDL_bool clip_enabled;
    float scale_x, scale_y;
    int logical_w, logical_h;
};

/* Internal texture structure */
struct SDL_Texture {
    SDL_Renderer *renderer;
    Uint32 format;
    int access;
    int w, h;
    int pitch;
    void *pixels;               /* Pixel data in ARGB8888 format */
    Uint8 r, g, b, a;           /* Color/alpha mod */
    SDL_BlendMode blend_mode;
    SDL_bool locked;
};

/* ========== Video Subsystem State ========== */

static struct {
    int initialized;
    int screen_w, screen_h;     /* Screen dimensions in pixels */
    SDL_Window *windows[16];    /* Max 16 windows */
    int window_count;
    Uint32 next_window_id;
    const char *error;
} video_state = {
    .initialized = 0,
    .screen_w = 0,
    .screen_h = 0,
    .window_count = 0,
    .next_window_id = 1,
    .error = NULL
};

/* Static pixel format for ARGB8888 */
static SDL_PixelFormat argb8888_format = {
    .format = SDL_PIXELFORMAT_ARGB8888,
    .palette = NULL,
    .BitsPerPixel = 32,
    .BytesPerPixel = 4,
    .Rmask = 0x00FF0000,
    .Gmask = 0x0000FF00,
    .Bmask = 0x000000FF,
    .Amask = 0xFF000000,
    .Rloss = 0, .Gloss = 0, .Bloss = 0, .Aloss = 0,
    .Rshift = 16, .Gshift = 8, .Bshift = 0, .Ashift = 24,
    .refcount = 1,
    .next = NULL
};

/* ========== Helper Functions ========== */

static void get_screen_size(int *w, int *h) {
    struct winsize ws;

    /* Try to get screen size via ioctl */
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_xpixel > 0 && ws.ws_ypixel > 0) {
        *w = ws.ws_xpixel;
        *h = ws.ws_ypixel;
    } else {
        /* Default fallback */
        *w = 640;
        *h = 480;
    }
}

static Uint32 color_to_argb(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    return ((Uint32)a << 24) | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
}

/* Convert ARGB8888 pixel buffer to RGBA for VOS sys_gfx_blit_rgba
 *
 * SDL ARGB8888: 0xAARRGGBB (A in high byte, B in low byte)
 * VOS RGBA (stb_image format): R at byte 0, G at byte 1, B at byte 2, A at byte 3
 * As a little-endian Uint32: 0xAABBGGRR
 */
static void convert_argb_to_rgba(const Uint32 *src, Uint32 *dst, int count) {
    for (int i = 0; i < count; i++) {
        Uint32 argb = src[i];
        Uint8 a = (argb >> 24) & 0xFF;
        Uint8 r = (argb >> 16) & 0xFF;
        Uint8 g = (argb >> 8) & 0xFF;
        Uint8 b = argb & 0xFF;
        /* Convert to RGBA little-endian: R in byte 0, A in byte 3 */
        dst[i] = ((Uint32)a << 24) | ((Uint32)b << 16) | ((Uint32)g << 8) | (Uint32)r;
    }
}

/* ========== Video Subsystem Functions ========== */

int SDL_GetNumVideoDrivers(void) {
    return 1;  /* VOS driver only */
}

const char* SDL_GetVideoDriver(int index) {
    if (index == 0) {
        return "vos";
    }
    return NULL;
}

int SDL_VideoInit(const char *driver_name) {
    (void)driver_name;  /* Ignore driver name, always use VOS */

    if (video_state.initialized) {
        return 0;  /* Already initialized */
    }

    /* Check if framebuffer mode is available */
    if (!sys_screen_is_fb()) {
        video_state.error = "SDL_VideoInit: VOS framebuffer not available";
        return -1;
    }

    /* Get screen dimensions */
    get_screen_size(&video_state.screen_w, &video_state.screen_h);

    /* Enable double buffering */
    sys_gfx_double_buffer(1);

    video_state.initialized = 1;
    video_state.error = NULL;

    return 0;
}

void SDL_VideoQuit(void) {
    int i;

    /* Destroy all windows */
    for (i = 0; i < video_state.window_count; i++) {
        if (video_state.windows[i]) {
            SDL_DestroyWindow(video_state.windows[i]);
            video_state.windows[i] = NULL;
        }
    }

    /* Disable double buffering */
    sys_gfx_double_buffer(0);

    video_state.initialized = 0;
    video_state.window_count = 0;
}

const char* SDL_GetCurrentVideoDriver(void) {
    if (video_state.initialized) {
        return "vos";
    }
    return NULL;
}

int SDL_GetNumVideoDisplays(void) {
    return 1;  /* Single display */
}

const char* SDL_GetDisplayName(int displayIndex) {
    if (displayIndex == 0) {
        return "VOS Display";
    }
    return NULL;
}

int SDL_GetDesktopDisplayMode(int displayIndex, SDL_DisplayMode *mode) {
    if (displayIndex != 0 || !mode) {
        return -1;
    }

    get_screen_size(&mode->w, &mode->h);
    mode->format = SDL_PIXELFORMAT_ARGB8888;
    mode->refresh_rate = 60;
    mode->driverdata = NULL;

    return 0;
}

int SDL_GetCurrentDisplayMode(int displayIndex, SDL_DisplayMode *mode) {
    return SDL_GetDesktopDisplayMode(displayIndex, mode);
}

/* ========== Window Functions ========== */

SDL_Window* SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags) {
    SDL_Window *window;

    if (!video_state.initialized) {
        if (SDL_VideoInit(NULL) < 0) {
            return NULL;
        }
    }

    if (video_state.window_count >= 16) {
        video_state.error = "SDL_CreateWindow: too many windows";
        return NULL;
    }

    /* Allocate window structure */
    window = (SDL_Window *)malloc(sizeof(SDL_Window));
    if (!window) {
        video_state.error = "SDL_CreateWindow: out of memory";
        return NULL;
    }

    memset(window, 0, sizeof(SDL_Window));

    /* Set window properties */
    window->id = video_state.next_window_id++;
    if (title) {
        strncpy(window->title, title, sizeof(window->title) - 1);
    }

    /* Handle special position values */
    if (SDL_WINDOWPOS_ISUNDEFINED(x) || SDL_WINDOWPOS_ISCENTERED(x)) {
        x = 0;
    }
    if (SDL_WINDOWPOS_ISUNDEFINED(y) || SDL_WINDOWPOS_ISCENTERED(y)) {
        y = 0;
    }

    window->x = x;
    window->y = y;

    /* Handle fullscreen - use screen size */
    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) {
        window->w = video_state.screen_w;
        window->h = video_state.screen_h;
        window->x = 0;
        window->y = 0;
    } else {
        window->w = (w > 0) ? w : video_state.screen_w;
        window->h = (h > 0) ? h : video_state.screen_h;
    }

    window->flags = flags | SDL_WINDOW_SHOWN;
    window->surface = NULL;
    window->renderer = NULL;

    /* Add to window list */
    video_state.windows[video_state.window_count++] = window;

    return window;
}

void SDL_DestroyWindow(SDL_Window *window) {
    int i;

    if (!window) {
        return;
    }

    /* Destroy associated renderer */
    if (window->renderer) {
        SDL_DestroyRenderer(window->renderer);
        window->renderer = NULL;
    }

    /* Free window surface */
    if (window->surface) {
        SDL_FreeSurface(window->surface);
        window->surface = NULL;
    }

    /* Remove from window list */
    for (i = 0; i < video_state.window_count; i++) {
        if (video_state.windows[i] == window) {
            /* Shift remaining windows */
            for (int j = i; j < video_state.window_count - 1; j++) {
                video_state.windows[j] = video_state.windows[j + 1];
            }
            video_state.window_count--;
            break;
        }
    }

    free(window);
}

SDL_Surface* SDL_GetWindowSurface(SDL_Window *window) {
    if (!window) {
        return NULL;
    }

    /* Create surface if it doesn't exist */
    if (!window->surface) {
        window->surface = SDL_CreateRGBSurfaceWithFormat(
            0, window->w, window->h, 32, SDL_PIXELFORMAT_ARGB8888);
    }

    return window->surface;
}

int SDL_UpdateWindowSurface(SDL_Window *window) {
    if (!window || !window->surface) {
        return -1;
    }

    /* Convert ARGB to RGBA and blit to VOS framebuffer */
    int pixel_count = window->surface->w * window->surface->h;
    Uint32 *rgba_buffer = (Uint32 *)malloc(pixel_count * 4);

    if (!rgba_buffer) {
        video_state.error = "SDL_UpdateWindowSurface: out of memory";
        return -1;
    }

    convert_argb_to_rgba((const Uint32 *)window->surface->pixels, rgba_buffer, pixel_count);

    /* Blit to VOS framebuffer */
    sys_gfx_blit_rgba(window->x, window->y, window->surface->w, window->surface->h, rgba_buffer);

    /* Flip the double buffer */
    sys_gfx_flip();

    free(rgba_buffer);

    return 0;
}

int SDL_UpdateWindowSurfaceRects(SDL_Window *window, const SDL_Rect *rects, int numrects) {
    (void)rects;
    (void)numrects;
    /* For simplicity, just update the entire surface */
    return SDL_UpdateWindowSurface(window);
}

void SDL_GetWindowSize(SDL_Window *window, int *w, int *h) {
    if (!window) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    if (w) *w = window->w;
    if (h) *h = window->h;
}

void SDL_SetWindowSize(SDL_Window *window, int w, int h) {
    if (!window) {
        return;
    }
    window->w = w;
    window->h = h;

    /* Recreate surface with new size */
    if (window->surface) {
        SDL_FreeSurface(window->surface);
        window->surface = NULL;
    }
}

void SDL_GetWindowPosition(SDL_Window *window, int *x, int *y) {
    if (!window) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    if (x) *x = window->x;
    if (y) *y = window->y;
}

void SDL_SetWindowPosition(SDL_Window *window, int x, int y) {
    if (!window) {
        return;
    }
    window->x = x;
    window->y = y;
}

const char* SDL_GetWindowTitle(SDL_Window *window) {
    if (!window) {
        return "";
    }
    return window->title;
}

void SDL_SetWindowTitle(SDL_Window *window, const char *title) {
    if (!window) {
        return;
    }
    if (title) {
        strncpy(window->title, title, sizeof(window->title) - 1);
    }
}

void SDL_SetWindowMinimumSize(SDL_Window *window, int min_w, int min_h) {
    if (window) {
        window->min_w = min_w;
        window->min_h = min_h;
    }
}

void SDL_GetWindowMinimumSize(SDL_Window *window, int *w, int *h) {
    if (window) {
        if (w) *w = window->min_w;
        if (h) *h = window->min_h;
    } else {
        if (w) *w = 0;
        if (h) *h = 0;
    }
}

void SDL_ShowWindow(SDL_Window *window) {
    if (window) {
        window->flags |= SDL_WINDOW_SHOWN;
        window->flags &= ~SDL_WINDOW_HIDDEN;
    }
}

void SDL_HideWindow(SDL_Window *window) {
    if (window) {
        window->flags &= ~SDL_WINDOW_SHOWN;
        window->flags |= SDL_WINDOW_HIDDEN;
    }
}

void SDL_RaiseWindow(SDL_Window *window) {
    (void)window;  /* No-op for VOS */
}

void SDL_MinimizeWindow(SDL_Window *window) {
    if (window) {
        window->flags |= SDL_WINDOW_MINIMIZED;
    }
}

void SDL_MaximizeWindow(SDL_Window *window) {
    if (window) {
        window->flags |= SDL_WINDOW_MAXIMIZED;
        window->w = video_state.screen_w;
        window->h = video_state.screen_h;
    }
}

void SDL_RestoreWindow(SDL_Window *window) {
    if (window) {
        window->flags &= ~(SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED);
    }
}

int SDL_SetWindowFullscreen(SDL_Window *window, Uint32 flags) {
    if (!window) {
        return -1;
    }

    if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) {
        window->flags |= flags;
        window->x = 0;
        window->y = 0;
        window->w = video_state.screen_w;
        window->h = video_state.screen_h;
    } else {
        window->flags &= ~(SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    return 0;
}

Uint32 SDL_GetWindowFlags(SDL_Window *window) {
    if (!window) {
        return 0;
    }
    return window->flags;
}

Uint32 SDL_GetWindowID(SDL_Window *window) {
    if (!window) {
        return 0;
    }
    return window->id;
}

SDL_Window* SDL_GetWindowFromID(Uint32 id) {
    for (int i = 0; i < video_state.window_count; i++) {
        if (video_state.windows[i] && video_state.windows[i]->id == id) {
            return video_state.windows[i];
        }
    }
    return NULL;
}

/* ========== Surface Functions ========== */

SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                   Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
    SDL_Surface *surface;
    int pitch;

    (void)flags;

    if (width <= 0 || height <= 0) {
        return NULL;
    }

    surface = (SDL_Surface *)malloc(sizeof(SDL_Surface));
    if (!surface) {
        return NULL;
    }

    memset(surface, 0, sizeof(SDL_Surface));

    /* Calculate pitch (bytes per row, aligned to 4 bytes) */
    pitch = ((width * (depth / 8) + 3) / 4) * 4;

    surface->flags = 0;
    surface->w = width;
    surface->h = height;
    surface->pitch = pitch;
    surface->pixels = malloc(pitch * height);

    if (!surface->pixels) {
        free(surface);
        return NULL;
    }

    memset(surface->pixels, 0, pitch * height);

    /* Set up pixel format */
    surface->format = &argb8888_format;  /* Always use ARGB8888 for simplicity */
    surface->clip_rect.x = 0;
    surface->clip_rect.y = 0;
    surface->clip_rect.w = width;
    surface->clip_rect.h = height;
    surface->refcount = 1;

    (void)Rmask; (void)Gmask; (void)Bmask; (void)Amask;

    return surface;
}

SDL_Surface* SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth, int pitch,
                                       Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
    SDL_Surface *surface;

    if (!pixels || width <= 0 || height <= 0) {
        return NULL;
    }

    surface = (SDL_Surface *)malloc(sizeof(SDL_Surface));
    if (!surface) {
        return NULL;
    }

    memset(surface, 0, sizeof(SDL_Surface));

    surface->flags = SDL_PREALLOC;  /* Pixels are not owned by surface */
    surface->w = width;
    surface->h = height;
    surface->pitch = pitch;
    surface->pixels = pixels;
    surface->format = &argb8888_format;
    surface->clip_rect.x = 0;
    surface->clip_rect.y = 0;
    surface->clip_rect.w = width;
    surface->clip_rect.h = height;
    surface->refcount = 1;

    (void)depth; (void)Rmask; (void)Gmask; (void)Bmask; (void)Amask;

    return surface;
}

SDL_Surface* SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth, Uint32 format) {
    (void)format;  /* Always use ARGB8888 */
    return SDL_CreateRGBSurface(flags, width, height, depth, 0, 0, 0, 0);
}

SDL_Surface* SDL_CreateRGBSurfaceWithFormatFrom(void *pixels, int width, int height, int depth,
                                                  int pitch, Uint32 format) {
    (void)format;  /* Always use ARGB8888 */
    return SDL_CreateRGBSurfaceFrom(pixels, width, height, depth, pitch, 0, 0, 0, 0);
}

void SDL_FreeSurface(SDL_Surface *surface) {
    if (!surface) {
        return;
    }

    surface->refcount--;
    if (surface->refcount > 0) {
        return;
    }

    /* Free pixel data if we own it */
    if (surface->pixels && !(surface->flags & SDL_PREALLOC)) {
        free(surface->pixels);
    }

    free(surface);
}

int SDL_SetColorKey(SDL_Surface *surface, int flag, Uint32 key) {
    (void)surface; (void)flag; (void)key;
    return 0;  /* Not implemented */
}

int SDL_GetColorKey(SDL_Surface *surface, Uint32 *key) {
    (void)surface; (void)key;
    return -1;  /* Not implemented */
}

int SDL_SetSurfaceAlphaMod(SDL_Surface *surface, Uint8 alpha) {
    (void)surface; (void)alpha;
    return 0;
}

int SDL_GetSurfaceAlphaMod(SDL_Surface *surface, Uint8 *alpha) {
    if (alpha) *alpha = 255;
    (void)surface;
    return 0;
}

int SDL_SetSurfaceBlendMode(SDL_Surface *surface, int blendMode) {
    (void)surface; (void)blendMode;
    return 0;
}

int SDL_GetSurfaceBlendMode(SDL_Surface *surface, int *blendMode) {
    if (blendMode) *blendMode = SDL_BLENDMODE_NONE;
    (void)surface;
    return 0;
}

SDL_bool SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect) {
    if (!surface) {
        return SDL_FALSE;
    }

    if (rect) {
        surface->clip_rect = *rect;
    } else {
        surface->clip_rect.x = 0;
        surface->clip_rect.y = 0;
        surface->clip_rect.w = surface->w;
        surface->clip_rect.h = surface->h;
    }

    return SDL_TRUE;
}

void SDL_GetClipRect(SDL_Surface *surface, SDL_Rect *rect) {
    if (surface && rect) {
        *rect = surface->clip_rect;
    }
}

int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color) {
    int x, y, x1, y1, x2, y2;
    Uint32 *pixels;

    if (!dst || !dst->pixels) {
        return -1;
    }

    if (rect) {
        x1 = rect->x;
        y1 = rect->y;
        x2 = rect->x + rect->w;
        y2 = rect->y + rect->h;
    } else {
        x1 = 0;
        y1 = 0;
        x2 = dst->w;
        y2 = dst->h;
    }

    /* Clip to surface bounds */
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > dst->w) x2 = dst->w;
    if (y2 > dst->h) y2 = dst->h;

    pixels = (Uint32 *)dst->pixels;

    for (y = y1; y < y2; y++) {
        Uint32 *row = pixels + (y * dst->pitch / 4);
        for (x = x1; x < x2; x++) {
            row[x] = color;
        }
    }

    return 0;
}

int SDL_FillRects(SDL_Surface *dst, const SDL_Rect *rects, int count, Uint32 color) {
    int i;
    for (i = 0; i < count; i++) {
        if (SDL_FillRect(dst, &rects[i], color) < 0) {
            return -1;
        }
    }
    return 0;
}

int SDL_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect,
                  SDL_Surface *dst, SDL_Rect *dstrect) {
    int sx, sy, dx, dy, w, h;
    Uint32 *src_pixels, *dst_pixels;

    if (!src || !dst || !src->pixels || !dst->pixels) {
        return -1;
    }

    /* Determine source rectangle */
    if (srcrect) {
        sx = srcrect->x;
        sy = srcrect->y;
        w = srcrect->w;
        h = srcrect->h;
    } else {
        sx = 0;
        sy = 0;
        w = src->w;
        h = src->h;
    }

    /* Determine destination position */
    if (dstrect) {
        dx = dstrect->x;
        dy = dstrect->y;
    } else {
        dx = 0;
        dy = 0;
    }

    /* Clip to destination surface */
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (dx + w > dst->w) { w = dst->w - dx; }
    if (dy + h > dst->h) { h = dst->h - dy; }

    /* Clip to source surface */
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    if (sx + w > src->w) { w = src->w - sx; }
    if (sy + h > src->h) { h = src->h - sy; }

    if (w <= 0 || h <= 0) {
        return 0;
    }

    src_pixels = (Uint32 *)src->pixels;
    dst_pixels = (Uint32 *)dst->pixels;

    /* Simple copy blit */
    for (int y = 0; y < h; y++) {
        Uint32 *src_row = src_pixels + ((sy + y) * src->pitch / 4) + sx;
        Uint32 *dst_row = dst_pixels + ((dy + y) * dst->pitch / 4) + dx;
        memcpy(dst_row, src_row, w * 4);
    }

    /* Update dstrect with actual dimensions */
    if (dstrect) {
        dstrect->w = w;
        dstrect->h = h;
    }

    return 0;
}

int SDL_BlitScaled(SDL_Surface *src, const SDL_Rect *srcrect,
                   SDL_Surface *dst, SDL_Rect *dstrect) {
    /* Simple implementation: just do a regular blit for now */
    return SDL_UpperBlit(src, srcrect, dst, dstrect);
}

int SDL_LockSurface(SDL_Surface *surface) {
    if (surface) {
        surface->locked = 1;
    }
    return 0;
}

void SDL_UnlockSurface(SDL_Surface *surface) {
    if (surface) {
        surface->locked = 0;
    }
}

int SDL_SoftStretch(SDL_Surface *src, const SDL_Rect *srcrect,
                    SDL_Surface *dst, const SDL_Rect *dstrect) {
    /* Simplified: just do a regular blit */
    return SDL_UpperBlit(src, srcrect, dst, (SDL_Rect *)dstrect);
}

SDL_Surface* SDL_ConvertSurface(SDL_Surface *src, const SDL_PixelFormat *fmt, Uint32 flags) {
    SDL_Surface *dst;

    (void)fmt; (void)flags;

    if (!src) {
        return NULL;
    }

    dst = SDL_CreateRGBSurfaceWithFormat(0, src->w, src->h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (dst) {
        SDL_UpperBlit(src, NULL, dst, NULL);
    }

    return dst;
}

SDL_Surface* SDL_ConvertSurfaceFormat(SDL_Surface *src, Uint32 pixel_format, Uint32 flags) {
    (void)pixel_format;
    return SDL_ConvertSurface(src, NULL, flags);
}

/* ========== Renderer Functions ========== */

int SDL_GetNumRenderDrivers(void) {
    return 1;
}

int SDL_GetRenderDriverInfo(int index, SDL_RendererInfo *info) {
    if (index != 0 || !info) {
        return -1;
    }

    info->name = "vos_software";
    info->flags = SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE;
    info->num_texture_formats = 1;
    info->texture_formats[0] = SDL_PIXELFORMAT_ARGB8888;
    info->max_texture_width = 4096;
    info->max_texture_height = 4096;

    return 0;
}

SDL_Renderer* SDL_CreateRenderer(SDL_Window *window, int index, Uint32 flags) {
    SDL_Renderer *renderer;

    (void)index; (void)flags;

    if (!window) {
        return NULL;
    }

    /* Only one renderer per window */
    if (window->renderer) {
        return window->renderer;
    }

    renderer = (SDL_Renderer *)malloc(sizeof(SDL_Renderer));
    if (!renderer) {
        return NULL;
    }

    memset(renderer, 0, sizeof(SDL_Renderer));

    renderer->window = window;
    renderer->target = SDL_GetWindowSurface(window);
    renderer->r = 0;
    renderer->g = 0;
    renderer->b = 0;
    renderer->a = 255;
    renderer->blend_mode = SDL_BLENDMODE_NONE;
    renderer->viewport.x = 0;
    renderer->viewport.y = 0;
    renderer->viewport.w = window->w;
    renderer->viewport.h = window->h;
    renderer->clip_enabled = SDL_FALSE;
    renderer->scale_x = 1.0f;
    renderer->scale_y = 1.0f;
    renderer->logical_w = 0;
    renderer->logical_h = 0;

    window->renderer = renderer;

    return renderer;
}

SDL_Renderer* SDL_CreateSoftwareRenderer(SDL_Surface *surface) {
    SDL_Renderer *renderer;

    if (!surface) {
        return NULL;
    }

    renderer = (SDL_Renderer *)malloc(sizeof(SDL_Renderer));
    if (!renderer) {
        return NULL;
    }

    memset(renderer, 0, sizeof(SDL_Renderer));

    renderer->window = NULL;
    renderer->target = surface;
    renderer->r = 0;
    renderer->g = 0;
    renderer->b = 0;
    renderer->a = 255;
    renderer->blend_mode = SDL_BLENDMODE_NONE;
    renderer->viewport.x = 0;
    renderer->viewport.y = 0;
    renderer->viewport.w = surface->w;
    renderer->viewport.h = surface->h;
    renderer->clip_enabled = SDL_FALSE;
    renderer->scale_x = 1.0f;
    renderer->scale_y = 1.0f;

    return renderer;
}

SDL_Renderer* SDL_GetRenderer(SDL_Window *window) {
    if (!window) {
        return NULL;
    }
    return window->renderer;
}

int SDL_GetRendererInfo(SDL_Renderer *renderer, SDL_RendererInfo *info) {
    (void)renderer;
    return SDL_GetRenderDriverInfo(0, info);
}

int SDL_GetRendererOutputSize(SDL_Renderer *renderer, int *w, int *h) {
    if (!renderer || !renderer->target) {
        return -1;
    }
    if (w) *w = renderer->target->w;
    if (h) *h = renderer->target->h;
    return 0;
}

void SDL_DestroyRenderer(SDL_Renderer *renderer) {
    if (!renderer) {
        return;
    }

    if (renderer->window) {
        renderer->window->renderer = NULL;
    }

    free(renderer);
}

/* ========== Texture Functions ========== */

SDL_Texture* SDL_CreateTexture(SDL_Renderer *renderer, Uint32 format,
                                int access, int w, int h) {
    SDL_Texture *texture;

    if (!renderer || w <= 0 || h <= 0) {
        return NULL;
    }

    texture = (SDL_Texture *)malloc(sizeof(SDL_Texture));
    if (!texture) {
        return NULL;
    }

    memset(texture, 0, sizeof(SDL_Texture));

    texture->renderer = renderer;
    texture->format = format ? format : SDL_PIXELFORMAT_ARGB8888;
    texture->access = access;
    texture->w = w;
    texture->h = h;
    texture->pitch = w * 4;
    texture->pixels = malloc(texture->pitch * h);
    texture->r = 255;
    texture->g = 255;
    texture->b = 255;
    texture->a = 255;
    texture->blend_mode = SDL_BLENDMODE_NONE;
    texture->locked = SDL_FALSE;

    if (!texture->pixels) {
        free(texture);
        return NULL;
    }

    memset(texture->pixels, 0, texture->pitch * h);

    return texture;
}

SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface) {
    SDL_Texture *texture;

    if (!renderer || !surface) {
        return NULL;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STATIC, surface->w, surface->h);
    if (!texture) {
        return NULL;
    }

    /* Copy surface pixels to texture */
    SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);

    return texture;
}

int SDL_QueryTexture(SDL_Texture *texture, Uint32 *format, int *access, int *w, int *h) {
    if (!texture) {
        return -1;
    }

    if (format) *format = texture->format;
    if (access) *access = texture->access;
    if (w) *w = texture->w;
    if (h) *h = texture->h;

    return 0;
}

int SDL_SetTextureColorMod(SDL_Texture *texture, Uint8 r, Uint8 g, Uint8 b) {
    if (!texture) {
        return -1;
    }
    texture->r = r;
    texture->g = g;
    texture->b = b;
    return 0;
}

int SDL_GetTextureColorMod(SDL_Texture *texture, Uint8 *r, Uint8 *g, Uint8 *b) {
    if (!texture) {
        return -1;
    }
    if (r) *r = texture->r;
    if (g) *g = texture->g;
    if (b) *b = texture->b;
    return 0;
}

int SDL_SetTextureAlphaMod(SDL_Texture *texture, Uint8 alpha) {
    if (!texture) {
        return -1;
    }
    texture->a = alpha;
    return 0;
}

int SDL_GetTextureAlphaMod(SDL_Texture *texture, Uint8 *alpha) {
    if (!texture) {
        return -1;
    }
    if (alpha) *alpha = texture->a;
    return 0;
}

int SDL_SetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode blendMode) {
    if (!texture) {
        return -1;
    }
    texture->blend_mode = blendMode;
    return 0;
}

int SDL_GetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode *blendMode) {
    if (!texture) {
        return -1;
    }
    if (blendMode) *blendMode = texture->blend_mode;
    return 0;
}

int SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect,
                      const void *pixels, int pitch) {
    int x, y, w, h;
    const Uint8 *src;
    Uint8 *dst;

    if (!texture || !pixels) {
        return -1;
    }

    if (rect) {
        x = rect->x;
        y = rect->y;
        w = rect->w;
        h = rect->h;
    } else {
        x = 0;
        y = 0;
        w = texture->w;
        h = texture->h;
    }

    src = (const Uint8 *)pixels;
    dst = (Uint8 *)texture->pixels + y * texture->pitch + x * 4;

    for (int row = 0; row < h; row++) {
        memcpy(dst, src, w * 4);
        src += pitch;
        dst += texture->pitch;
    }

    return 0;
}

int SDL_UpdateYUVTexture(SDL_Texture *texture, const SDL_Rect *rect,
                          const Uint8 *Yplane, int Ypitch,
                          const Uint8 *Uplane, int Upitch,
                          const Uint8 *Vplane, int Vpitch) {
    (void)texture; (void)rect;
    (void)Yplane; (void)Ypitch;
    (void)Uplane; (void)Upitch;
    (void)Vplane; (void)Vpitch;
    return -1;  /* Not implemented */
}

int SDL_LockTexture(SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch) {
    (void)rect;

    if (!texture || !pixels || !pitch) {
        return -1;
    }

    if (texture->access != SDL_TEXTUREACCESS_STREAMING) {
        return -1;
    }

    *pixels = texture->pixels;
    *pitch = texture->pitch;
    texture->locked = SDL_TRUE;

    return 0;
}

void SDL_UnlockTexture(SDL_Texture *texture) {
    if (texture) {
        texture->locked = SDL_FALSE;
    }
}

void SDL_DestroyTexture(SDL_Texture *texture) {
    if (!texture) {
        return;
    }

    if (texture->pixels) {
        free(texture->pixels);
    }

    free(texture);
}

/* ========== Render Target Functions ========== */

int SDL_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture) {
    if (!renderer) {
        return -1;
    }

    if (texture) {
        /* Create a temporary surface from texture for rendering */
        /* For simplicity, we don't fully support render targets */
        return -1;
    } else {
        /* Reset to window surface */
        if (renderer->window) {
            renderer->target = renderer->window->surface;
        }
    }

    return 0;
}

SDL_Texture* SDL_GetRenderTarget(SDL_Renderer *renderer) {
    (void)renderer;
    return NULL;  /* We don't track texture targets */
}

/* ========== Drawing Functions ========== */

int SDL_SetRenderDrawColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if (!renderer) {
        return -1;
    }
    renderer->r = r;
    renderer->g = g;
    renderer->b = b;
    renderer->a = a;
    return 0;
}

int SDL_GetRenderDrawColor(SDL_Renderer *renderer, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a) {
    if (!renderer) {
        return -1;
    }
    if (r) *r = renderer->r;
    if (g) *g = renderer->g;
    if (b) *b = renderer->b;
    if (a) *a = renderer->a;
    return 0;
}

int SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode) {
    if (!renderer) {
        return -1;
    }
    renderer->blend_mode = blendMode;
    return 0;
}

int SDL_GetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode *blendMode) {
    if (!renderer) {
        return -1;
    }
    if (blendMode) *blendMode = renderer->blend_mode;
    return 0;
}

int SDL_RenderClear(SDL_Renderer *renderer) {
    Uint32 color;

    if (!renderer || !renderer->target) {
        return -1;
    }

    color = color_to_argb(renderer->r, renderer->g, renderer->b, renderer->a);
    SDL_FillRect(renderer->target, NULL, color);

    return 0;
}

int SDL_RenderDrawPoint(SDL_Renderer *renderer, int x, int y) {
    Uint32 color;
    Uint32 *pixels;

    if (!renderer || !renderer->target || !renderer->target->pixels) {
        return -1;
    }

    /* Apply viewport offset */
    x += renderer->viewport.x;
    y += renderer->viewport.y;

    /* Bounds check */
    if (x < 0 || x >= renderer->target->w || y < 0 || y >= renderer->target->h) {
        return 0;
    }

    color = color_to_argb(renderer->r, renderer->g, renderer->b, renderer->a);
    pixels = (Uint32 *)renderer->target->pixels;
    pixels[y * (renderer->target->pitch / 4) + x] = color;

    return 0;
}

int SDL_RenderDrawPoints(SDL_Renderer *renderer, const SDL_Point *points, int count) {
    int i;
    for (i = 0; i < count; i++) {
        SDL_RenderDrawPoint(renderer, points[i].x, points[i].y);
    }
    return 0;
}

int SDL_RenderDrawLine(SDL_Renderer *renderer, int x1, int y1, int x2, int y2) {
    int dx, dy, sx, sy, err, e2;

    if (!renderer) {
        return -1;
    }

    /* Bresenham's line algorithm */
    dx = x2 > x1 ? x2 - x1 : x1 - x2;
    dy = y2 > y1 ? y2 - y1 : y1 - y2;
    sx = x1 < x2 ? 1 : -1;
    sy = y1 < y2 ? 1 : -1;
    err = dx - dy;

    while (1) {
        SDL_RenderDrawPoint(renderer, x1, y1);

        if (x1 == x2 && y1 == y2) {
            break;
        }

        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }

    return 0;
}

int SDL_RenderDrawLines(SDL_Renderer *renderer, const SDL_Point *points, int count) {
    int i;
    for (i = 0; i < count - 1; i++) {
        SDL_RenderDrawLine(renderer, points[i].x, points[i].y,
                          points[i + 1].x, points[i + 1].y);
    }
    return 0;
}

int SDL_RenderDrawRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
    SDL_Rect r;

    if (!renderer) {
        return -1;
    }

    if (rect) {
        r = *rect;
    } else {
        r.x = 0;
        r.y = 0;
        r.w = renderer->viewport.w;
        r.h = renderer->viewport.h;
    }

    /* Draw four lines */
    SDL_RenderDrawLine(renderer, r.x, r.y, r.x + r.w - 1, r.y);
    SDL_RenderDrawLine(renderer, r.x + r.w - 1, r.y, r.x + r.w - 1, r.y + r.h - 1);
    SDL_RenderDrawLine(renderer, r.x + r.w - 1, r.y + r.h - 1, r.x, r.y + r.h - 1);
    SDL_RenderDrawLine(renderer, r.x, r.y + r.h - 1, r.x, r.y);

    return 0;
}

int SDL_RenderDrawRects(SDL_Renderer *renderer, const SDL_Rect *rects, int count) {
    int i;
    for (i = 0; i < count; i++) {
        SDL_RenderDrawRect(renderer, &rects[i]);
    }
    return 0;
}

int SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
    Uint32 color;
    SDL_Rect r;

    if (!renderer || !renderer->target) {
        return -1;
    }

    if (rect) {
        r = *rect;
        r.x += renderer->viewport.x;
        r.y += renderer->viewport.y;
    } else {
        r = renderer->viewport;
    }

    color = color_to_argb(renderer->r, renderer->g, renderer->b, renderer->a);
    SDL_FillRect(renderer->target, &r, color);

    return 0;
}

int SDL_RenderFillRects(SDL_Renderer *renderer, const SDL_Rect *rects, int count) {
    int i;
    for (i = 0; i < count; i++) {
        SDL_RenderFillRect(renderer, &rects[i]);
    }
    return 0;
}

int SDL_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture,
                   const SDL_Rect *srcrect, const SDL_Rect *dstrect) {
    int sx, sy, sw, sh;
    int dx, dy, dw, dh;

    if (!renderer || !renderer->target || !texture || !texture->pixels) {
        return -1;
    }

    /* Source rectangle */
    if (srcrect) {
        sx = srcrect->x;
        sy = srcrect->y;
        sw = srcrect->w;
        sh = srcrect->h;
    } else {
        sx = 0;
        sy = 0;
        sw = texture->w;
        sh = texture->h;
    }

    /* Destination rectangle */
    if (dstrect) {
        dx = dstrect->x + renderer->viewport.x;
        dy = dstrect->y + renderer->viewport.y;
        dw = dstrect->w;
        dh = dstrect->h;
    } else {
        dx = renderer->viewport.x;
        dy = renderer->viewport.y;
        dw = renderer->viewport.w;
        dh = renderer->viewport.h;
    }

    /* Simple non-scaled blit if dimensions match */
    if (sw == dw && sh == dh) {
        Uint32 *src_pixels = (Uint32 *)texture->pixels;
        Uint32 *dst_pixels = (Uint32 *)renderer->target->pixels;
        int dst_pitch = renderer->target->pitch / 4;
        int src_pitch = texture->pitch / 4;

        /* Clip to target bounds */
        int x1 = dx < 0 ? 0 : dx;
        int y1 = dy < 0 ? 0 : dy;
        int x2 = (dx + dw) > renderer->target->w ? renderer->target->w : (dx + dw);
        int y2 = (dy + dh) > renderer->target->h ? renderer->target->h : (dy + dh);

        int src_x_offset = x1 - dx;
        int src_y_offset = y1 - dy;

        for (int y = y1; y < y2; y++) {
            int src_y = sy + src_y_offset + (y - y1);
            if (src_y < 0 || src_y >= texture->h) continue;

            Uint32 *src_row = src_pixels + src_y * src_pitch;
            Uint32 *dst_row = dst_pixels + y * dst_pitch;

            for (int x = x1; x < x2; x++) {
                int src_x = sx + src_x_offset + (x - x1);
                if (src_x < 0 || src_x >= texture->w) continue;

                Uint32 pixel = src_row[src_x];

                /* Apply alpha blending if needed */
                if (texture->blend_mode == SDL_BLENDMODE_BLEND) {
                    Uint8 sa = (pixel >> 24) & 0xFF;
                    if (sa == 0) continue;  /* Fully transparent */
                    if (sa < 255) {
                        /* Blend with destination */
                        Uint32 dpixel = dst_row[x];
                        Uint8 sr = (pixel >> 16) & 0xFF;
                        Uint8 sg = (pixel >> 8) & 0xFF;
                        Uint8 sb = pixel & 0xFF;
                        Uint8 dr = (dpixel >> 16) & 0xFF;
                        Uint8 dg = (dpixel >> 8) & 0xFF;
                        Uint8 db = dpixel & 0xFF;

                        Uint8 r = (sr * sa + dr * (255 - sa)) / 255;
                        Uint8 g = (sg * sa + dg * (255 - sa)) / 255;
                        Uint8 b = (sb * sa + db * (255 - sa)) / 255;

                        pixel = 0xFF000000 | ((Uint32)r << 16) | ((Uint32)g << 8) | b;
                    }
                }

                dst_row[x] = pixel;
            }
        }
    }

    return 0;
}

int SDL_RenderCopyEx(SDL_Renderer *renderer, SDL_Texture *texture,
                     const SDL_Rect *srcrect, const SDL_Rect *dstrect,
                     const double angle, const SDL_Point *center,
                     const SDL_RendererFlip flip) {
    (void)angle; (void)center; (void)flip;
    /* Simplified: ignore rotation and flip */
    return SDL_RenderCopy(renderer, texture, srcrect, dstrect);
}

int SDL_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
                          Uint32 format, void *pixels, int pitch) {
    (void)format;

    if (!renderer || !renderer->target || !pixels) {
        return -1;
    }

    SDL_Rect r;
    if (rect) {
        r = *rect;
    } else {
        r.x = 0;
        r.y = 0;
        r.w = renderer->target->w;
        r.h = renderer->target->h;
    }

    Uint8 *src = (Uint8 *)renderer->target->pixels + r.y * renderer->target->pitch + r.x * 4;
    Uint8 *dst = (Uint8 *)pixels;

    for (int y = 0; y < r.h; y++) {
        memcpy(dst, src, r.w * 4);
        src += renderer->target->pitch;
        dst += pitch;
    }

    return 0;
}

void SDL_RenderPresent(SDL_Renderer *renderer) {
    if (!renderer || !renderer->window) {
        return;
    }

    /* Update the window surface to the screen */
    SDL_UpdateWindowSurface(renderer->window);
}

int SDL_RenderSetScale(SDL_Renderer *renderer, float scaleX, float scaleY) {
    if (!renderer) {
        return -1;
    }
    renderer->scale_x = scaleX;
    renderer->scale_y = scaleY;
    return 0;
}

void SDL_RenderGetScale(SDL_Renderer *renderer, float *scaleX, float *scaleY) {
    if (!renderer) {
        if (scaleX) *scaleX = 1.0f;
        if (scaleY) *scaleY = 1.0f;
        return;
    }
    if (scaleX) *scaleX = renderer->scale_x;
    if (scaleY) *scaleY = renderer->scale_y;
}

int SDL_RenderSetViewport(SDL_Renderer *renderer, const SDL_Rect *rect) {
    if (!renderer) {
        return -1;
    }

    if (rect) {
        renderer->viewport = *rect;
    } else {
        renderer->viewport.x = 0;
        renderer->viewport.y = 0;
        if (renderer->target) {
            renderer->viewport.w = renderer->target->w;
            renderer->viewport.h = renderer->target->h;
        }
    }

    return 0;
}

void SDL_RenderGetViewport(SDL_Renderer *renderer, SDL_Rect *rect) {
    if (renderer && rect) {
        *rect = renderer->viewport;
    }
}

int SDL_RenderSetClipRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
    if (!renderer) {
        return -1;
    }

    if (rect) {
        renderer->clip_rect = *rect;
        renderer->clip_enabled = SDL_TRUE;
    } else {
        renderer->clip_enabled = SDL_FALSE;
    }

    return 0;
}

void SDL_RenderGetClipRect(SDL_Renderer *renderer, SDL_Rect *rect) {
    if (renderer && rect) {
        *rect = renderer->clip_rect;
    }
}

SDL_bool SDL_RenderIsClipEnabled(SDL_Renderer *renderer) {
    if (!renderer) {
        return SDL_FALSE;
    }
    return renderer->clip_enabled;
}

int SDL_RenderSetIntegerScale(SDL_Renderer *renderer, SDL_bool enable) {
    (void)renderer; (void)enable;
    return 0;
}

SDL_bool SDL_RenderGetIntegerScale(SDL_Renderer *renderer) {
    (void)renderer;
    return SDL_FALSE;
}

int SDL_RenderSetLogicalSize(SDL_Renderer *renderer, int w, int h) {
    if (!renderer) {
        return -1;
    }
    renderer->logical_w = w;
    renderer->logical_h = h;
    return 0;
}

void SDL_RenderGetLogicalSize(SDL_Renderer *renderer, int *w, int *h) {
    if (!renderer) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    if (w) *w = renderer->logical_w;
    if (h) *h = renderer->logical_h;
}

/* ========== Pixel Format Functions ========== */

const char* SDL_GetPixelFormatName(Uint32 format) {
    switch (format) {
        case SDL_PIXELFORMAT_ARGB8888: return "SDL_PIXELFORMAT_ARGB8888";
        case SDL_PIXELFORMAT_RGBA8888: return "SDL_PIXELFORMAT_RGBA8888";
        case SDL_PIXELFORMAT_ABGR8888: return "SDL_PIXELFORMAT_ABGR8888";
        case SDL_PIXELFORMAT_BGRA8888: return "SDL_PIXELFORMAT_BGRA8888";
        case SDL_PIXELFORMAT_RGB888:   return "SDL_PIXELFORMAT_RGB888";
        case SDL_PIXELFORMAT_BGR888:   return "SDL_PIXELFORMAT_BGR888";
        case SDL_PIXELFORMAT_RGB565:   return "SDL_PIXELFORMAT_RGB565";
        default: return "SDL_PIXELFORMAT_UNKNOWN";
    }
}

SDL_PixelFormat* SDL_AllocFormat(Uint32 pixel_format) {
    SDL_PixelFormat *format;

    format = (SDL_PixelFormat *)malloc(sizeof(SDL_PixelFormat));
    if (!format) {
        return NULL;
    }

    memset(format, 0, sizeof(SDL_PixelFormat));
    format->format = pixel_format;
    format->BitsPerPixel = SDL_BITSPERPIXEL(pixel_format);
    format->BytesPerPixel = SDL_BYTESPERPIXEL(pixel_format);

    /* Set up masks for ARGB8888 */
    if (pixel_format == SDL_PIXELFORMAT_ARGB8888) {
        format->Amask = 0xFF000000;
        format->Rmask = 0x00FF0000;
        format->Gmask = 0x0000FF00;
        format->Bmask = 0x000000FF;
        format->Ashift = 24;
        format->Rshift = 16;
        format->Gshift = 8;
        format->Bshift = 0;
    }

    format->refcount = 1;

    return format;
}

void SDL_FreeFormat(SDL_PixelFormat *format) {
    if (format && format != &argb8888_format) {
        format->refcount--;
        if (format->refcount <= 0) {
            free(format);
        }
    }
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b) {
    if (!format) {
        return 0;
    }
    return ((Uint32)r << format->Rshift) |
           ((Uint32)g << format->Gshift) |
           ((Uint32)b << format->Bshift) |
           format->Amask;  /* Full alpha */
}

Uint32 SDL_MapRGBA(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if (!format) {
        return 0;
    }
    return ((Uint32)a << format->Ashift) |
           ((Uint32)r << format->Rshift) |
           ((Uint32)g << format->Gshift) |
           ((Uint32)b << format->Bshift);
}

void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *format, Uint8 *r, Uint8 *g, Uint8 *b) {
    if (!format) {
        if (r) *r = 0;
        if (g) *g = 0;
        if (b) *b = 0;
        return;
    }
    if (r) *r = (pixel & format->Rmask) >> format->Rshift;
    if (g) *g = (pixel & format->Gmask) >> format->Gshift;
    if (b) *b = (pixel & format->Bmask) >> format->Bshift;
}

void SDL_GetRGBA(Uint32 pixel, const SDL_PixelFormat *format, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a) {
    SDL_GetRGB(pixel, format, r, g, b);
    if (a && format) {
        if (format->Amask) {
            *a = (pixel & format->Amask) >> format->Ashift;
        } else {
            *a = 255;
        }
    }
}

/* ========== Rectangle Functions ========== */

SDL_bool SDL_HasIntersection(const SDL_Rect *A, const SDL_Rect *B) {
    if (!A || !B) {
        return SDL_FALSE;
    }

    if (A->x + A->w <= B->x || B->x + B->w <= A->x ||
        A->y + A->h <= B->y || B->y + B->h <= A->y) {
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

SDL_bool SDL_IntersectRect(const SDL_Rect *A, const SDL_Rect *B, SDL_Rect *result) {
    int Amin, Amax, Bmin, Bmax;

    if (!A || !B || !result) {
        return SDL_FALSE;
    }

    /* X intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin) Amin = Bmin;
    if (Bmax < Amax) Amax = Bmax;
    if (Amax <= Amin) return SDL_FALSE;
    result->x = Amin;
    result->w = Amax - Amin;

    /* Y intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin) Amin = Bmin;
    if (Bmax < Amax) Amax = Bmax;
    if (Amax <= Amin) return SDL_FALSE;
    result->y = Amin;
    result->h = Amax - Amin;

    return SDL_TRUE;
}

void SDL_UnionRect(const SDL_Rect *A, const SDL_Rect *B, SDL_Rect *result) {
    int Amin, Amax, Bmin, Bmax;

    if (!A || !B || !result) {
        return;
    }

    /* X union */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin < Amin) Amin = Bmin;
    if (Bmax > Amax) Amax = Bmax;
    result->x = Amin;
    result->w = Amax - Amin;

    /* Y union */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin < Amin) Amin = Bmin;
    if (Bmax > Amax) Amax = Bmax;
    result->y = Amin;
    result->h = Amax - Amin;
}

SDL_bool SDL_EnclosePoints(const SDL_Point *points, int count,
                           const SDL_Rect *clip, SDL_Rect *result) {
    int minx = 0, miny = 0, maxx = 0, maxy = 0;
    int i;
    SDL_bool added = SDL_FALSE;

    if (!points || count <= 0) {
        return SDL_FALSE;
    }

    for (i = 0; i < count; i++) {
        int x = points[i].x;
        int y = points[i].y;

        if (clip) {
            if (x < clip->x || x >= clip->x + clip->w ||
                y < clip->y || y >= clip->y + clip->h) {
                continue;
            }
        }

        if (!added) {
            minx = maxx = x;
            miny = maxy = y;
            added = SDL_TRUE;
        } else {
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        }
    }

    if (!added) {
        return SDL_FALSE;
    }

    if (result) {
        result->x = minx;
        result->y = miny;
        result->w = maxx - minx + 1;
        result->h = maxy - miny + 1;
    }

    return SDL_TRUE;
}

SDL_bool SDL_IntersectRectAndLine(const SDL_Rect *rect,
                                   int *X1, int *Y1, int *X2, int *Y2) {
    (void)rect; (void)X1; (void)Y1; (void)X2; (void)Y2;
    /* Not fully implemented */
    return SDL_TRUE;
}

/* ========== Memory Functions (SDL_stdinc.h) ========== */

void *SDL_malloc(size_t size) {
    return malloc(size);
}

void *SDL_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void *SDL_realloc(void *mem, size_t size) {
    return realloc(mem, size);
}

void SDL_free(void *mem) {
    free(mem);
}

size_t SDL_strlen(const char *str) {
    return strlen(str);
}

char *SDL_strcpy(char *dst, const char *src) {
    return strcpy(dst, src);
}

char *SDL_strncpy(char *dst, const char *src, size_t maxlen) {
    return strncpy(dst, src, maxlen);
}

int SDL_strcmp(const char *str1, const char *str2) {
    return strcmp(str1, str2);
}

int SDL_strncmp(const char *str1, const char *str2, size_t maxlen) {
    return strncmp(str1, str2, maxlen);
}

void *SDL_memset(void *dst, int c, size_t len) {
    return memset(dst, c, len);
}

void *SDL_memcpy(void *dst, const void *src, size_t len) {
    return memcpy(dst, src, len);
}

void *SDL_memmove(void *dst, const void *src, size_t len) {
    return memmove(dst, src, len);
}

int SDL_memcmp(const void *s1, const void *s2, size_t len) {
    return memcmp(s1, s2, len);
}

/* ========== Palette Functions ========== */

SDL_Palette* SDL_AllocPalette(int ncolors) {
    SDL_Palette *palette;

    palette = (SDL_Palette *)malloc(sizeof(SDL_Palette));
    if (!palette) {
        return NULL;
    }

    palette->colors = (SDL_Color *)calloc(ncolors, sizeof(SDL_Color));
    if (!palette->colors) {
        free(palette);
        return NULL;
    }

    palette->ncolors = ncolors;
    palette->version = 1;
    palette->refcount = 1;

    /* Initialize with grayscale */
    for (int i = 0; i < ncolors; i++) {
        palette->colors[i].r = i * 255 / (ncolors - 1);
        palette->colors[i].g = palette->colors[i].r;
        palette->colors[i].b = palette->colors[i].r;
        palette->colors[i].a = 255;
    }

    return palette;
}

int SDL_SetPixelFormatPalette(SDL_PixelFormat *format, SDL_Palette *palette) {
    if (!format) {
        return -1;
    }
    format->palette = palette;
    return 0;
}

int SDL_SetPaletteColors(SDL_Palette *palette, const SDL_Color *colors, int firstcolor, int ncolors) {
    if (!palette || !colors) {
        return -1;
    }

    if (firstcolor < 0 || firstcolor + ncolors > palette->ncolors) {
        return -1;
    }

    memcpy(&palette->colors[firstcolor], colors, ncolors * sizeof(SDL_Color));
    palette->version++;

    return 0;
}

void SDL_FreePalette(SDL_Palette *palette) {
    if (!palette) {
        return;
    }

    palette->refcount--;
    if (palette->refcount <= 0) {
        if (palette->colors) {
            free(palette->colors);
        }
        free(palette);
    }
}

/* ========== BMP Loader ========== */

/* BMP file header structure */
#pragma pack(push, 1)
typedef struct {
    Uint16 bfType;          /* "BM" */
    Uint32 bfSize;          /* File size */
    Uint16 bfReserved1;
    Uint16 bfReserved2;
    Uint32 bfOffBits;       /* Offset to pixel data */
} BMPFileHeader;

typedef struct {
    Uint32 biSize;          /* Size of this header (40 for BITMAPINFOHEADER) */
    Sint32 biWidth;
    Sint32 biHeight;        /* Positive = bottom-up, negative = top-down */
    Uint16 biPlanes;
    Uint16 biBitCount;
    Uint32 biCompression;
    Uint32 biSizeImage;
    Sint32 biXPelsPerMeter;
    Sint32 biYPelsPerMeter;
    Uint32 biClrUsed;
    Uint32 biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

SDL_Surface* SDL_LoadBMP_RW(SDL_RWops *src, int freesrc)
{
    BMPFileHeader file_hdr;
    BMPInfoHeader info_hdr;
    SDL_Surface *surface = NULL;
    Uint8 *row_buffer = NULL;
    int width, height, pitch;
    int topdown = 0;
    int row_size;
    Uint32 Rmask, Gmask, Bmask, Amask;

    if (src == NULL) {
        return NULL;
    }

    /* Read file header */
    if (SDL_RWread(src, &file_hdr, sizeof(file_hdr), 1) != 1) {
        goto done;
    }

    /* Check BMP signature */
    if (file_hdr.bfType != 0x4D42) {  /* "BM" in little-endian */
        goto done;
    }

    /* Read info header */
    if (SDL_RWread(src, &info_hdr, sizeof(info_hdr), 1) != 1) {
        goto done;
    }

    width = info_hdr.biWidth;
    height = info_hdr.biHeight;

    /* Negative height means top-down DIB */
    if (height < 0) {
        height = -height;
        topdown = 1;
    }

    /* We only support 24-bit and 32-bit uncompressed BMPs for simplicity */
    if (info_hdr.biCompression != 0 ||
        (info_hdr.biBitCount != 24 && info_hdr.biBitCount != 32)) {
        goto done;
    }

    /* Set up masks for ARGB8888 surface */
    Rmask = 0x00FF0000;
    Gmask = 0x0000FF00;
    Bmask = 0x000000FF;
    Amask = 0xFF000000;

    /* Create the surface */
    surface = SDL_CreateRGBSurface(0, width, height, 32, Rmask, Gmask, Bmask, Amask);
    if (surface == NULL) {
        goto done;
    }

    /* Seek to pixel data */
    SDL_RWseek(src, file_hdr.bfOffBits, RW_SEEK_SET);

    /* BMP rows are padded to 4-byte boundaries */
    row_size = ((width * info_hdr.biBitCount + 31) / 32) * 4;
    row_buffer = (Uint8 *)malloc(row_size);
    if (row_buffer == NULL) {
        SDL_FreeSurface(surface);
        surface = NULL;
        goto done;
    }

    pitch = surface->pitch;

    /* Read pixel data */
    for (int y = 0; y < height; y++) {
        int dest_y = topdown ? y : (height - 1 - y);
        Uint32 *dst = (Uint32 *)((Uint8 *)surface->pixels + dest_y * pitch);

        if (SDL_RWread(src, row_buffer, row_size, 1) != 1) {
            SDL_FreeSurface(surface);
            surface = NULL;
            goto done;
        }

        if (info_hdr.biBitCount == 24) {
            /* 24-bit BGR -> ARGB */
            Uint8 *src_ptr = row_buffer;
            for (int x = 0; x < width; x++) {
                Uint8 b = *src_ptr++;
                Uint8 g = *src_ptr++;
                Uint8 r = *src_ptr++;
                dst[x] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        } else {
            /* 32-bit BGRA -> ARGB */
            Uint8 *src_ptr = row_buffer;
            for (int x = 0; x < width; x++) {
                Uint8 b = *src_ptr++;
                Uint8 g = *src_ptr++;
                Uint8 r = *src_ptr++;
                Uint8 a = *src_ptr++;
                dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }

done:
    if (row_buffer) {
        free(row_buffer);
    }
    if (freesrc && src) {
        SDL_RWclose(src);
    }
    return surface;
}
