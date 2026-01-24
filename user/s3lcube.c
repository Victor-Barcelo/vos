#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <syscall.h>

static bool get_fb_px(int* out_w, int* out_h) {
    if (!out_w || !out_h) {
        return false;
    }
    *out_w = 0;
    *out_h = 0;

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != 0) {
        return false;
    }
    if (ws.ws_xpixel == 0 || ws.ws_ypixel == 0) {
        return false;
    }
    *out_w = (int)ws.ws_xpixel;
    *out_h = (int)ws.ws_ypixel;
    return true;
}

static int reserved_bottom_px(void) {
    int idx = sys_font_get_current();
    if (idx < 0) {
        return 0;
    }
    vos_font_info_t info;
    memset(&info, 0, sizeof(info));
    if (sys_font_info((uint32_t)idx, &info) != 0) {
        return 0;
    }
    if (info.height == 0) {
        return 0;
    }
    return (int)info.height;
}

static struct termios g_termios_orig;
static bool g_have_termios = false;

static void raw_mode_begin(void) {
    if (tcgetattr(STDIN_FILENO, &g_termios_orig) == 0) {
        g_have_termios = true;
        struct termios raw = g_termios_orig;
        cfmakeraw(&raw);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    (void)write(STDOUT_FILENO, "\033[?25l", 6); // hide cursor
}

static void raw_mode_end(void) {
    (void)write(STDOUT_FILENO, "\033[?25h", 6); // show cursor
    if (g_have_termios) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &g_termios_orig);
    }
}

#define RGBA(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

#define S3L_PIXEL_FUNCTION s3l_draw_pixel
#define S3L_RESOLUTION_X 640
#define S3L_RESOLUTION_Y 480
#define S3L_Z_BUFFER 1
#include <small3d.h>

static uint32_t fb[S3L_RESOLUTION_X * S3L_RESOLUTION_Y];

static const uint32_t tri_colors[S3L_CUBE_TRIANGLE_COUNT] = {
    RGBA(27, 27, 27, 255),    // front
    RGBA(42, 42, 42, 255),
    RGBA(0, 51, 102, 255),    // right
    RGBA(0, 64, 128, 255),
    RGBA(64, 0, 43, 255),     // back
    RGBA(85, 0, 58, 255),
    RGBA(0, 85, 34, 255),     // left
    RGBA(0, 122, 51, 255),
    RGBA(102, 102, 0, 255),   // top
    RGBA(128, 128, 0, 255),
    RGBA(85, 0, 0, 255),      // bottom
    RGBA(119, 0, 0, 255),
};

static inline void s3l_draw_pixel(S3L_PixelInfo* p) {
    fb[p->y * S3L_RESOLUTION_X + p->x] = tri_colors[p->triangleIndex % S3L_CUBE_TRIANGLE_COUNT];
}

int main(int argc, char** argv) {
    uint32_t max_ms = 0;
    if (argc >= 2 && argv && argv[1]) {
        int v = atoi(argv[1]);
        if (v > 0) {
            max_ms = (uint32_t)v;
        }
    }

    if (sys_screen_is_fb() != 1) {
        puts("s3lcube: framebuffer console not available");
        return 1;
    }

    int fb_w = 0;
    int fb_h = 0;
    if (!get_fb_px(&fb_w, &fb_h)) {
        puts("s3lcube: could not query framebuffer size");
        return 1;
    }

    int reserved = reserved_bottom_px();
    if (reserved > 0 && reserved < fb_h) {
        fb_h -= reserved;
    }

    if (fb_w < (int)S3L_RESOLUTION_X || fb_h < (int)S3L_RESOLUTION_Y) {
        printf("s3lcube: screen too small (%dx%d px), need at least %dx%d\n",
               fb_w, fb_h, (int)S3L_RESOLUTION_X, (int)S3L_RESOLUTION_Y);
        return 1;
    }

    int out_x = (fb_w - (int)S3L_RESOLUTION_X) / 2;
    int out_y = (fb_h - (int)S3L_RESOLUTION_Y) / 2;
    if (out_x < 0) {
        out_x = 0;
    }
    if (out_y < 0) {
        out_y = 0;
    }

    static const S3L_Unit cube_vertices[S3L_CUBE_VERTEX_COUNT * 3] = {
        S3L_CUBE_VERTICES(S3L_F),
    };
    static const S3L_Index cube_tris[S3L_CUBE_TRIANGLE_COUNT * 3] = {
        S3L_CUBE_TRIANGLES,
    };

    S3L_Model3D model;
    S3L_model3DInit(cube_vertices, S3L_CUBE_VERTEX_COUNT, cube_tris, S3L_CUBE_TRIANGLE_COUNT, &model);
    model.transform.translation.z = 4 * S3L_F;

    S3L_Scene scene;
    S3L_sceneInit(&model, 1, &scene);

    raw_mode_begin();
    (void)sys_gfx_clear(0); // VGA palette index 0 (black)

    uint32_t start_ms = sys_uptime_ms();

    while (1) {
        uint32_t ms = sys_uptime_ms();
        if (max_ms != 0 && (ms - start_ms) >= max_ms) {
            break;
        }
        memset(fb, 0, sizeof(fb));

        // Full rotation every ~6 seconds.
        S3L_Unit a = (S3L_Unit)((ms * (uint32_t)S3L_F) / 6000u);
        model.transform.rotation.x = a;
        model.transform.rotation.y = a / 2;
        model.transform.rotation.z = a / 3;

        S3L_newFrame();
        S3L_drawScene(scene);

        // Draw a thin border.
        for (int x = 0; x < (int)S3L_RESOLUTION_X; x++) {
            fb[x] = RGBA(245, 245, 245, 255);
            fb[(S3L_RESOLUTION_Y - 1) * S3L_RESOLUTION_X + x] = RGBA(245, 245, 245, 255);
        }
        for (int y = 0; y < (int)S3L_RESOLUTION_Y; y++) {
            fb[y * S3L_RESOLUTION_X + 0] = RGBA(245, 245, 245, 255);
            fb[y * S3L_RESOLUTION_X + (S3L_RESOLUTION_X - 1)] = RGBA(245, 245, 245, 255);
        }

        (void)sys_gfx_blit_rgba(out_x, out_y, S3L_RESOLUTION_X, S3L_RESOLUTION_Y, fb);

        uint8_t b = 0;
        ssize_t n = read(STDIN_FILENO, &b, 1);
        if (n == 1 && (b == 27 || b == 'q' || b == 'Q')) {
            break;
        } else if (n < 0 && errno != EAGAIN) {
            break;
        }

        (void)sys_sleep(16);
    }

    raw_mode_end();
    return 0;
}
