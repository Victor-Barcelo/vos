#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "syscall.h"

#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static uint32_t u32_min(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static int get_framebuffer(uint32_t* out_w, uint32_t* out_h) {
    if (!out_w || !out_h) {
        return -1;
    }
    *out_w = 0;
    *out_h = 0;

    if (sys_screen_is_fb() != 1) {
        return -1;
    }

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    if (ioctl(0, TIOCGWINSZ, &ws) != 0) {
        return -1;
    }
    if (ws.ws_xpixel == 0 || ws.ws_ypixel == 0) {
        return -1;
    }

    *out_w = (uint32_t)ws.ws_xpixel;
    *out_h = (uint32_t)ws.ws_ypixel;
    return 0;
}

static uint32_t get_reserved_bottom_px(void) {
    int idx = sys_font_get_current();
    if (idx < 0) {
        return 0;
    }
    vos_font_info_t info;
    memset(&info, 0, sizeof(info));
    if (sys_font_info((uint32_t)idx, &info) != 0) {
        return 0;
    }
    // Status bar reserves 1 text row.
    return info.height;
}

static void nearest_scale_rgba(uint8_t* out, uint32_t out_w, uint32_t out_h,
                               const uint8_t* in, uint32_t in_w, uint32_t in_h) {
    for (uint32_t y = 0; y < out_h; y++) {
        uint32_t sy = (uint32_t)(((uint64_t)y * (uint64_t)in_h) / (uint64_t)out_h);
        if (sy >= in_h) sy = in_h - 1u;
        for (uint32_t x = 0; x < out_w; x++) {
            uint32_t sx = (uint32_t)(((uint64_t)x * (uint64_t)in_w) / (uint64_t)out_w);
            if (sx >= in_w) sx = in_w - 1u;
            const uint8_t* sp = in + (sy * in_w + sx) * 4u;
            uint8_t* dp = out + (y * out_w + x) * 4u;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            dp[3] = sp[3];
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2 || !argv[1] || argv[1][0] == '\0') {
        puts("Usage: img <file>");
        puts("Supported formats: png, jpg, bmp, tga, gif, psd, pnm (via stb_image)");
        return 1;
    }

    uint32_t fb_w = 0;
    uint32_t fb_h = 0;
    if (get_framebuffer(&fb_w, &fb_h) != 0) {
        puts("img: framebuffer mode required");
        return 1;
    }

    uint32_t reserved_bottom = get_reserved_bottom_px();
    uint32_t usable_h = (reserved_bottom < fb_h) ? (fb_h - reserved_bottom) : fb_h;
    if (usable_h == 0) {
        puts("img: no usable screen height");
        return 1;
    }

    int iw = 0;
    int ih = 0;
    int comp = 0;
    uint8_t* pixels = (uint8_t*)stbi_load(argv[1], &iw, &ih, &comp, 4);
    if (!pixels) {
        printf("img: failed to load '%s'\n", argv[1]);
        return 1;
    }
    if (iw <= 0 || ih <= 0) {
        stbi_image_free(pixels);
        puts("img: invalid image dimensions");
        return 1;
    }

    uint32_t in_w = (uint32_t)iw;
    uint32_t in_h = (uint32_t)ih;

    // Fit the image inside the usable screen area (no upscale).
    uint32_t out_w = in_w;
    uint32_t out_h = in_h;
    if (out_w > fb_w || out_h > usable_h) {
        out_w = fb_w;
        out_h = (uint32_t)(((uint64_t)in_h * (uint64_t)out_w) / (uint64_t)in_w);
        if (out_h > usable_h) {
            out_h = usable_h;
            out_w = (uint32_t)(((uint64_t)in_w * (uint64_t)out_h) / (uint64_t)in_h);
        }
        out_w = u32_min(out_w, fb_w);
        out_h = u32_min(out_h, usable_h);
        if (out_w == 0) out_w = 1;
        if (out_h == 0) out_h = 1;
    }

    uint8_t* out_pixels = pixels;
    bool need_free_out = false;
    if (out_w != in_w || out_h != in_h) {
        uint64_t bytes64 = (uint64_t)out_w * (uint64_t)out_h * 4u;
        if (bytes64 > 64u * 1024u * 1024u) {
            stbi_image_free(pixels);
            puts("img: scaled image too large");
            return 1;
        }
        out_pixels = (uint8_t*)malloc((size_t)bytes64);
        if (!out_pixels) {
            stbi_image_free(pixels);
            puts("img: out of memory");
            return 1;
        }
        nearest_scale_rgba(out_pixels, out_w, out_h, pixels, in_w, in_h);
        need_free_out = true;
    }

    (void)sys_gfx_clear(0);

    int32_t x0 = (int32_t)((fb_w - out_w) / 2u);
    int32_t y0 = (int32_t)((usable_h - out_h) / 2u);
    int rc = sys_gfx_blit_rgba(x0, y0, out_w, out_h, out_pixels);
    if (rc != 0) {
        printf("img: draw failed (rc=%d)\n", rc);
    }

    puts("Press 'q' to quit.");
    for (;;) {
        char c = 0;
        int n = (int)read(0, &c, 1);
        if (n <= 0) {
            break;
        }
        if (c == 'q' || c == 'Q') {
            break;
        }
    }

    // Clear back to a clean prompt.
    fputs("\x1b[2J\x1b[H", stdout);

    if (need_free_out) {
        free(out_pixels);
    }
    stbi_image_free(pixels);
    return 0;
}
