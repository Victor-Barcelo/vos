# Chapter 27: Graphics Programming

VOS provides framebuffer graphics support for user programs, enabling 2D graphics rendering through direct pixel manipulation.

## Framebuffer Architecture

### Console Modes

VOS supports two console modes:

1. **VGA Text Mode**: Traditional 80x25 character display
2. **Framebuffer Mode**: Linear pixel buffer for graphics

The framebuffer is initialized via GRUB's multiboot info when a video mode is requested.

### Framebuffer Structure

```c
typedef struct {
    uint32_t *buffer;       // Pixel buffer (BGRA format)
    uint32_t width;         // Width in pixels
    uint32_t height;        // Height in pixels
    uint32_t pitch;         // Bytes per scanline
    uint8_t bpp;            // Bits per pixel (typically 32)
} framebuffer_t;
```

## Graphics Syscalls

VOS provides system calls for graphics operations:

### sys_screen_is_fb

```c
// Check if framebuffer console is active
// Returns: 1 if framebuffer, 0 if text mode
int sys_screen_is_fb(void);
```

### sys_gfx_blit_rgba

```c
// Blit RGBA pixel data to framebuffer
// x, y: top-left position
// w, h: dimensions
// pixels: RGBA pixel data
int sys_gfx_blit_rgba(uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h,
                       const uint32_t *pixels);
```

### sys_font_info

```c
typedef struct {
    uint32_t width;
    uint32_t height;
    char name[64];
} vos_font_info_t;

// Get font information
int sys_font_info(uint32_t index, vos_font_info_t *info);
```

### sys_font_get_current

```c
// Get current font index
int sys_font_get_current(void);
```

## Olive.c Graphics Library

VOS includes [olive.c](https://github.com/tsoding/olive.c), a single-header 2D graphics library for software rendering.

### Features

- Rectangles and frames
- Circles and ellipses
- Lines (Bresenham algorithm)
- Triangles (filled)
- Text rendering (built-in font)
- Alpha blending
- No external dependencies

### Including Olive

```c
#include <olive.h>

// In ONE translation unit:
#define OLIVEC_IMPLEMENTATION
#include <olive.h>
```

### Canvas Creation

```c
// Allocate pixel buffer
size_t w = 800, h = 600;
uint32_t *pixels = malloc(w * h * sizeof(uint32_t));

// Create canvas
Olivec_Canvas oc = olivec_canvas(pixels, w, h, w);
```

### Drawing Primitives

```c
// Fill entire canvas
olivec_fill(oc, 0xFF000000);  // Black

// Draw rectangle
olivec_rect(oc, 100, 100, 200, 150, 0xFF0000FF);  // Blue

// Draw frame (outline)
olivec_frame(oc, 100, 100, 200, 150, 2, 0xFFFFFFFF);  // White

// Draw circle
olivec_circle(oc, 400, 300, 50, 0xFF00FF00);  // Green

// Draw line
olivec_line(oc, 0, 0, 799, 599, 0xFFFF00FF);  // Magenta

// Draw text
olivec_text(oc, "Hello VOS!", 10, 10, olivec_default_font, 2, 0xFFFFFFFF);
```

### Color Format

Olive uses RGBA format (0xAARRGGBB in little-endian):

```c
static inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r) |
           ((uint32_t)g << 8) |
           ((uint32_t)b << 16) |
           ((uint32_t)a << 24);
}

uint32_t white = rgba(255, 255, 255, 255);
uint32_t red   = rgba(255, 0, 0, 255);
uint32_t transparent_blue = rgba(0, 0, 255, 128);
```

## Complete Graphics Example

```c
#include <olive.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <syscall.h>

static struct termios g_orig;

static void raw_mode_begin(void) {
    tcgetattr(STDIN_FILENO, &g_orig);
    struct termios raw = g_orig;
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    write(STDOUT_FILENO, "\033[?25l", 6);  // Hide cursor
}

static void raw_mode_end(void) {
    write(STDOUT_FILENO, "\033[?25h", 6);  // Show cursor
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig);
}

int main(void) {
    // Check framebuffer availability
    if (sys_screen_is_fb() != 1) {
        puts("Framebuffer not available");
        return 1;
    }

    // Get screen size via ioctl
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != 0) {
        puts("Could not get screen size");
        return 1;
    }

    int w = ws.ws_xpixel;
    int h = ws.ws_ypixel;

    // Allocate pixel buffer
    uint32_t *pixels = malloc(w * h * sizeof(uint32_t));
    if (!pixels) {
        puts("malloc failed");
        return 1;
    }

    Olivec_Canvas oc = olivec_canvas(pixels, w, h, w);
    raw_mode_begin();

    // Animation loop
    uint32_t start = sys_uptime_ms();
    while (1) {
        uint32_t ms = sys_uptime_ms() - start;

        // Clear screen
        olivec_fill(oc, 0xFF000000);

        // Animated rectangle
        int box_x = (ms / 5) % (w + 100) - 50;
        olivec_rect(oc, box_x, h/2 - 40, 100, 80, 0xFF3264C8);
        olivec_frame(oc, box_x, h/2 - 40, 100, 80, 2, 0xFFFFFFFF);

        // Static circle
        olivec_circle(oc, w/2, h/2, 60, 0xFF1EDC8C);

        // Blit to screen
        sys_gfx_blit_rgba(0, 0, w, h, pixels);

        // Check for quit key
        uint8_t key;
        if (read(STDIN_FILENO, &key, 1) == 1) {
            if (key == 'q' || key == 27) break;
        }

        sys_sleep(16);  // ~60 FPS
    }

    raw_mode_end();
    free(pixels);
    return 0;
}
```

## Small3dlib

VOS also includes [small3dlib](https://gitlab.com/drummyfish/small3dlib), a software 3D renderer:

### Features

- Software rasterization
- Perspective projection
- Texture mapping
- No floating point required
- Single header library

### Example: 3D Cube

```c
#include <small3dlib.h>

// Define cube vertices and triangles
S3L_Unit cubeVertices[] = { ... };
S3L_Index cubeTriangles[] = { ... };

S3L_Model3D model;
S3L_Scene scene;

void pixelFunc(S3L_PixelInfo *p) {
    // Called for each pixel
    pixels[p->y * width + p->x] = color;
}

int main(void) {
    S3L_initCamera(&scene.camera);
    S3L_initModel3D(...);

    while (running) {
        S3L_newFrame();
        S3L_drawScene(scene);
        // Blit to screen
    }
}
```

## Performance Tips

### Double Buffering

Draw to an off-screen buffer, then blit once:

```c
// Bad: blit after each primitive
olivec_rect(oc, ...);
sys_gfx_blit_rgba(...);  // Slow!

// Good: blit once per frame
olivec_fill(oc, bg);
olivec_rect(oc, ...);
olivec_circle(oc, ...);
olivec_line(oc, ...);
sys_gfx_blit_rgba(...);  // Single blit
```

### Frame Rate Control

```c
#define TARGET_FPS 60
#define FRAME_TIME (1000 / TARGET_FPS)

uint32_t frame_start = sys_uptime_ms();
// ... draw frame ...
uint32_t elapsed = sys_uptime_ms() - frame_start;
if (elapsed < FRAME_TIME) {
    sys_sleep(FRAME_TIME - elapsed);
}
```

### Partial Updates

Only blit changed regions when possible:

```c
// Full screen blit (expensive)
sys_gfx_blit_rgba(0, 0, screen_w, screen_h, pixels);

// Partial blit (cheaper)
sys_gfx_blit_rgba(dirty_x, dirty_y, dirty_w, dirty_h, region);
```

## Status Bar Reservation

The bottom portion of the screen may be reserved for a status bar. Query the font height to avoid drawing over it:

```c
int reserved_bottom(void) {
    int idx = sys_font_get_current();
    if (idx < 0) return 0;

    vos_font_info_t info;
    if (sys_font_info(idx, &info) != 0) return 0;

    return info.height;  // Usually 16-32 pixels
}

// Adjust drawing area
int usable_height = screen_height - reserved_bottom();
```

## Building Graphics Programs

```makefile
CFLAGS = -I/usr/include -DOLIVEC_IMPLEMENTATION
LDFLAGS = -lolive -lc

olivedemo: olivedemo.c
    $(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
```

With TCC inside VOS:

```bash
tcc -o demo demo.c -lolive
```

## SDL2 Shim

VOS includes an SDL2 shim layer that provides a subset of SDL2 API for portable graphics code:

```c
#include <SDL2/SDL.h>

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *win = SDL_CreateWindow("VOS App",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600, 0);
    SDL_Renderer *rend = SDL_CreateRenderer(win, -1, 0);

    // Drawing loop
    SDL_SetRenderDrawColor(rend, 0, 0, 255, 255);
    SDL_RenderClear(rend);
    SDL_RenderPresent(rend);

    // ... event loop ...

    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
```

SDL_image is also available for loading images:

```c
#include <SDL2/SDL_image.h>

SDL_Surface *image = IMG_Load("/disk/images/photo.png");
```

## Summary

VOS graphics programming provides:

1. **Framebuffer access** via syscalls
2. **SDL2 shim** for portable graphics code
3. **SDL_image** for image loading (PNG, etc.)
4. **Olive.c** for 2D software rendering
5. **Small3dlib** for 3D software rendering
6. **Performance techniques** for smooth animation
7. **Status bar awareness** for proper screen usage

This enables graphical applications like games, visualizations, image viewers, and emulators.

---

*Previous: [Chapter 26: Newlib Integration](26_newlib.md)*
*Next: [Chapter 28: BASIC Interpreter](28_basic.md)*
