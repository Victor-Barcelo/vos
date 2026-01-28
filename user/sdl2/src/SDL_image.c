/*
 * SDL_image implementation for VOS
 *
 * Uses stb_image for PNG/JPG/BMP/TGA/GIF decoding.
 * Provides IMG_Load_RW and related functions needed by klystrack.
 */

#include "SDL2/SDL_image.h"
#include "SDL2/SDL_video.h"
#include "SDL2/SDL_rwops.h"
#include "SDL2/SDL_error.h"
#include <stdlib.h>
#include <string.h>

/* stb_image configuration */
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_STDIO  /* We use SDL_RWops, not FILE* */

/* Custom stb_image callbacks for SDL_RWops */
#define STBI_NO_FAILURE_STRINGS  /* Reduce size */

/* Implement stb_image */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* ========== stb_image callbacks for SDL_RWops ========== */

static int stbi_rw_read(void *user, char *data, int size) {
    SDL_RWops *rw = (SDL_RWops *)user;
    return (int)SDL_RWread(rw, data, 1, size);
}

static void stbi_rw_skip(void *user, int n) {
    SDL_RWops *rw = (SDL_RWops *)user;
    SDL_RWseek(rw, n, RW_SEEK_CUR);
}

static int stbi_rw_eof(void *user) {
    SDL_RWops *rw = (SDL_RWops *)user;
    Sint64 pos = SDL_RWtell(rw);
    Sint64 size = SDL_RWsize(rw);
    if (pos < 0 || size < 0) return 1;
    return (pos >= size) ? 1 : 0;
}

static stbi_io_callbacks rw_callbacks = {
    .read = stbi_rw_read,
    .skip = stbi_rw_skip,
    .eof = stbi_rw_eof
};

/* ========== Helper Functions ========== */

/*
 * Convert RGBA (stb_image output) to ARGB (SDL surface format)
 * Also handles pre-multiplied alpha if needed.
 */
static void convert_rgba_to_argb(const Uint8 *rgba, Uint32 *argb, int pixel_count) {
    for (int i = 0; i < pixel_count; i++) {
        Uint8 r = rgba[0];
        Uint8 g = rgba[1];
        Uint8 b = rgba[2];
        Uint8 a = rgba[3];
        *argb++ = ((Uint32)a << 24) | ((Uint32)r << 16) | ((Uint32)g << 8) | b;
        rgba += 4;
    }
}

/*
 * Create an SDL_Surface from RGBA pixel data
 */
static SDL_Surface* create_surface_from_rgba(Uint8 *rgba_data, int width, int height) {
    SDL_Surface *surface;
    int pitch;

    /* Create surface with ARGB8888 format */
    surface = SDL_CreateRGBSurface(0, width, height, 32,
                                   0x00FF0000,  /* R mask */
                                   0x0000FF00,  /* G mask */
                                   0x000000FF,  /* B mask */
                                   0xFF000000); /* A mask */

    if (!surface) {
        return NULL;
    }

    pitch = surface->pitch;

    /* Convert RGBA to ARGB row by row */
    for (int y = 0; y < height; y++) {
        const Uint8 *src_row = rgba_data + y * width * 4;
        Uint32 *dst_row = (Uint32 *)((Uint8 *)surface->pixels + y * pitch);
        convert_rgba_to_argb(src_row, dst_row, width);
    }

    return surface;
}

/* ========== Public API ========== */

int IMG_Init(int flags) {
    /* stb_image doesn't need initialization */
    return flags;
}

void IMG_Quit(void) {
    /* Nothing to clean up */
}

SDL_Surface* IMG_Load_RW(SDL_RWops *src, int freesrc) {
    SDL_Surface *surface = NULL;
    Uint8 *rgba_data = NULL;
    int width, height, channels;

    if (!src) {
        SDL_SetError("IMG_Load_RW: NULL SDL_RWops");
        return NULL;
    }

    /* Load image using stb_image with RWops callbacks */
    rgba_data = stbi_load_from_callbacks(&rw_callbacks, src, &width, &height, &channels, 4);

    if (!rgba_data) {
        SDL_SetError("IMG_Load_RW: stb_image failed to load image");
        if (freesrc) {
            SDL_RWclose(src);
        }
        return NULL;
    }

    /* Create SDL surface from RGBA data */
    surface = create_surface_from_rgba(rgba_data, width, height);

    /* Free stb_image data */
    stbi_image_free(rgba_data);

    if (!surface) {
        SDL_SetError("IMG_Load_RW: failed to create surface");
    }

    /* Close RWops if requested */
    if (freesrc) {
        SDL_RWclose(src);
    }

    return surface;
}

SDL_Surface* IMG_Load(const char *file) {
    SDL_RWops *rw;

    if (!file) {
        SDL_SetError("IMG_Load: NULL filename");
        return NULL;
    }

    rw = SDL_RWFromFile(file, "rb");
    if (!rw) {
        SDL_SetError("IMG_Load: cannot open file '%s'", file);
        return NULL;
    }

    return IMG_Load_RW(rw, 1);  /* freesrc = 1 */
}

/* ========== Format Detection ========== */

int IMG_isPNG(SDL_RWops *src) {
    Sint64 start;
    Uint8 magic[8];
    int is_png = 0;

    if (!src) return 0;

    start = SDL_RWtell(src);
    if (SDL_RWread(src, magic, 1, 8) == 8) {
        /* PNG magic: 89 50 4E 47 0D 0A 1A 0A */
        is_png = (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G' &&
                  magic[4] == 0x0D && magic[5] == 0x0A && magic[6] == 0x1A && magic[7] == 0x0A);
    }
    SDL_RWseek(src, start, RW_SEEK_SET);
    return is_png;
}

int IMG_isBMP(SDL_RWops *src) {
    Sint64 start;
    Uint8 magic[2];
    int is_bmp = 0;

    if (!src) return 0;

    start = SDL_RWtell(src);
    if (SDL_RWread(src, magic, 1, 2) == 2) {
        is_bmp = (magic[0] == 'B' && magic[1] == 'M');
    }
    SDL_RWseek(src, start, RW_SEEK_SET);
    return is_bmp;
}

int IMG_isJPG(SDL_RWops *src) {
    Sint64 start;
    Uint8 magic[3];
    int is_jpg = 0;

    if (!src) return 0;

    start = SDL_RWtell(src);
    if (SDL_RWread(src, magic, 1, 3) == 3) {
        /* JPEG magic: FF D8 FF */
        is_jpg = (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF);
    }
    SDL_RWseek(src, start, RW_SEEK_SET);
    return is_jpg;
}

int IMG_isGIF(SDL_RWops *src) {
    Sint64 start;
    Uint8 magic[6];
    int is_gif = 0;

    if (!src) return 0;

    start = SDL_RWtell(src);
    if (SDL_RWread(src, magic, 1, 6) == 6) {
        is_gif = (magic[0] == 'G' && magic[1] == 'I' && magic[2] == 'F' &&
                  magic[3] == '8' && (magic[4] == '7' || magic[4] == '9') && magic[5] == 'a');
    }
    SDL_RWseek(src, start, RW_SEEK_SET);
    return is_gif;
}

int IMG_isTIF(SDL_RWops *src) {
    Sint64 start;
    Uint8 magic[4];
    int is_tif = 0;

    if (!src) return 0;

    start = SDL_RWtell(src);
    if (SDL_RWread(src, magic, 1, 4) == 4) {
        /* TIFF magic: II (little-endian) or MM (big-endian) + 42 */
        is_tif = ((magic[0] == 'I' && magic[1] == 'I' && magic[2] == 42 && magic[3] == 0) ||
                  (magic[0] == 'M' && magic[1] == 'M' && magic[2] == 0 && magic[3] == 42));
    }
    SDL_RWseek(src, start, RW_SEEK_SET);
    return is_tif;
}

/* ========== Format-Specific Loaders ========== */

SDL_Surface* IMG_LoadPNG_RW(SDL_RWops *src) {
    return IMG_Load_RW(src, 0);  /* stb_image auto-detects format */
}

SDL_Surface* IMG_LoadBMP_RW(SDL_RWops *src) {
    return IMG_Load_RW(src, 0);
}

SDL_Surface* IMG_LoadJPG_RW(SDL_RWops *src) {
    return IMG_Load_RW(src, 0);
}

SDL_Surface* IMG_LoadGIF_RW(SDL_RWops *src) {
    return IMG_Load_RW(src, 0);
}

SDL_Surface* IMG_LoadTGA_RW(SDL_RWops *src) {
    return IMG_Load_RW(src, 0);
}
