/*
 * s3lfly - Simple 3D flying game for VOS
 * Navigate through space and collect cubes!
 * Controls: WASD to move, Q/E to rotate, ESC to quit
 */

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
    if (!out_w || !out_h) return false;
    *out_w = 0;
    *out_h = 0;
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != 0) return false;
    if (ws.ws_xpixel == 0 || ws.ws_ypixel == 0) return false;
    *out_w = (int)ws.ws_xpixel;
    *out_h = (int)ws.ws_ypixel;
    return true;
}

static int reserved_bottom_px(void) {
    int idx = sys_font_get_current();
    if (idx < 0) return 0;
    vos_font_info_t info;
    memset(&info, 0, sizeof(info));
    if (sys_font_info((uint32_t)idx, &info) != 0) return 0;
    return (info.height == 0) ? 0 : (int)info.height;
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
    (void)write(STDOUT_FILENO, "\033[?25l", 6);
}

static void raw_mode_end(void) {
    (void)write(STDOUT_FILENO, "\033[?25h", 6);
    if (g_have_termios) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &g_termios_orig);
    }
}

#define RGBA(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

#define S3L_PIXEL_FUNCTION s3l_draw_pixel
#define S3L_RESOLUTION_X 320
#define S3L_RESOLUTION_Y 240
#define S3L_Z_BUFFER 1
#define S3L_SORT 0
#define S3L_STENCIL_BUFFER 0
#define S3L_NEAR_CROSS_STRATEGY 1
#include <small3d.h>

static uint32_t fb[S3L_RESOLUTION_X * S3L_RESOLUTION_Y];
// Colors for different cubes
static const uint32_t model_colors[] = {
    RGBA(255, 50, 50, 255),   // Red - player
    RGBA(50, 255, 50, 255),   // Green - collectible
    RGBA(50, 100, 255, 255),  // Blue
    RGBA(255, 255, 50, 255),  // Yellow
    RGBA(255, 50, 255, 255),  // Magenta
    RGBA(50, 255, 255, 255),  // Cyan
};

static inline void s3l_draw_pixel(S3L_PixelInfo* p) {
    uint32_t color = model_colors[p->modelIndex % 6];
    // Simple depth shading
    int shade = 255 - (p->depth >> 4);
    if (shade < 50) shade = 50;
    uint8_t r = ((color & 0xFF) * shade) >> 8;
    uint8_t g = (((color >> 8) & 0xFF) * shade) >> 8;
    uint8_t b = (((color >> 16) & 0xFF) * shade) >> 8;
    fb[p->y * S3L_RESOLUTION_X + p->x] = RGBA(r, g, b, 255);
}

// Number of collectible cubes
#define NUM_CUBES 5

static S3L_Unit cube_vertices[] = { S3L_CUBE_VERTICES(S3L_F) };
static const S3L_Index cube_tris[] = { S3L_CUBE_TRIANGLES };

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    if (sys_screen_is_fb() != 1) {
        puts("s3lfly: framebuffer console not available");
        return 1;
    }

    int fb_w = 0, fb_h = 0;
    if (!get_fb_px(&fb_w, &fb_h)) {
        puts("s3lfly: could not query framebuffer size");
        return 1;
    }

    int reserved = reserved_bottom_px();
    if (reserved > 0 && reserved < fb_h) fb_h -= reserved;

    if (fb_w < (int)S3L_RESOLUTION_X || fb_h < (int)S3L_RESOLUTION_Y) {
        printf("s3lfly: screen too small (%dx%d)\n", fb_w, fb_h);
        return 1;
    }

    int out_x = (fb_w - (int)S3L_RESOLUTION_X) / 2;
    int out_y = (fb_h - (int)S3L_RESOLUTION_Y) / 2;
    if (out_x < 0) out_x = 0;
    if (out_y < 0) out_y = 0;

    // Create models - one for player, rest are collectibles
    S3L_Model3D models[NUM_CUBES + 1];

    // Player cube (model 0)
    S3L_model3DInit(cube_vertices, S3L_CUBE_VERTEX_COUNT, cube_tris, S3L_CUBE_TRIANGLE_COUNT, &models[0]);
    models[0].transform.translation.z = 0;
    models[0].transform.scale.x = S3L_F / 2;
    models[0].transform.scale.y = S3L_F / 2;
    models[0].transform.scale.z = S3L_F / 2;

    // Collectible cubes
    for (int i = 1; i <= NUM_CUBES; i++) {
        S3L_model3DInit(cube_vertices, S3L_CUBE_VERTEX_COUNT, cube_tris, S3L_CUBE_TRIANGLE_COUNT, &models[i]);
        models[i].transform.translation.x = ((i * 3) % 7 - 3) * S3L_F * 2;
        models[i].transform.translation.y = ((i * 5) % 5 - 2) * S3L_F;
        models[i].transform.translation.z = (i * 4 + 5) * S3L_F;
        models[i].transform.scale.x = S3L_F / 3;
        models[i].transform.scale.y = S3L_F / 3;
        models[i].transform.scale.z = S3L_F / 3;
    }

    S3L_Scene scene;
    S3L_sceneInit(models, NUM_CUBES + 1, &scene);

    // Position camera behind the player
    scene.camera.transform.translation.z = -5 * S3L_F;
    scene.camera.transform.translation.y = S3L_F;

    raw_mode_begin();
    (void)sys_gfx_clear(0);

    int score = 0;
    bool collected[NUM_CUBES + 1] = {false};
    S3L_Unit player_x = 0, player_y = 0, player_z = 0;
    S3L_Unit player_rot = 0;

    uint32_t start_ms = sys_uptime_ms();
    (void)start_ms;

    while (1) {
        uint32_t now = sys_uptime_ms();

        // Clear framebuffer with space background
        for (int i = 0; i < S3L_RESOLUTION_X * S3L_RESOLUTION_Y; i++) {
            // Simple starfield
            if ((i * 7) % 500 == 0) {
                fb[i] = RGBA(200, 200, 200, 255);
            } else {
                fb[i] = RGBA(5, 5, 20, 255);
            }
        }

        // Update collectible rotations
        for (int i = 1; i <= NUM_CUBES; i++) {
            if (!collected[i]) {
                models[i].transform.rotation.y = (S3L_Unit)((now * S3L_F) / 2000);
                models[i].transform.rotation.x = (S3L_Unit)((now * S3L_F) / 3000);
            }
        }

        // Update camera to follow player
        scene.camera.transform.translation.x = player_x;
        scene.camera.transform.translation.y = player_y + S3L_F;
        scene.camera.transform.translation.z = player_z - 5 * S3L_F;
        scene.camera.transform.rotation.y = player_rot;

        // Draw scene
        S3L_newFrame();
        S3L_drawScene(scene);

        // Draw border
        for (int x = 0; x < (int)S3L_RESOLUTION_X; x++) {
            fb[x] = RGBA(100, 100, 100, 255);
            fb[(S3L_RESOLUTION_Y - 1) * S3L_RESOLUTION_X + x] = RGBA(100, 100, 100, 255);
        }
        for (int y = 0; y < (int)S3L_RESOLUTION_Y; y++) {
            fb[y * S3L_RESOLUTION_X] = RGBA(100, 100, 100, 255);
            fb[y * S3L_RESOLUTION_X + S3L_RESOLUTION_X - 1] = RGBA(100, 100, 100, 255);
        }

        // Draw simple HUD - score in top left
        // (Just draw colored pixels for score indicator)
        for (int s = 0; s < score && s < 5; s++) {
            for (int py = 5; py < 15; py++) {
                for (int px = 5 + s * 12; px < 15 + s * 12; px++) {
                    fb[py * S3L_RESOLUTION_X + px] = RGBA(0, 255, 0, 255);
                }
            }
        }

        (void)sys_gfx_blit_rgba(out_x, out_y, S3L_RESOLUTION_X, S3L_RESOLUTION_Y, fb);

        // Check for collisions with collectibles
        for (int i = 1; i <= NUM_CUBES; i++) {
            if (!collected[i]) {
                S3L_Unit dx = models[i].transform.translation.x - player_x;
                S3L_Unit dy = models[i].transform.translation.y - player_y;
                S3L_Unit dz = models[i].transform.translation.z - player_z;
                // Simple distance check
                S3L_Unit dist2 = (dx >> 6) * (dx >> 6) + (dy >> 6) * (dy >> 6) + (dz >> 6) * (dz >> 6);
                if (dist2 < (S3L_F * S3L_F) >> 10) {
                    collected[i] = true;
                    // Move collected cube far away (hide it)
                    models[i].transform.translation.z = -1000 * S3L_F;
                    score++;
                    if (score >= NUM_CUBES) {
                        // Win! Show message briefly then exit
                        raw_mode_end();
                        printf("\n\033[32;1mYou collected all %d cubes!\033[0m\n", NUM_CUBES);
                        return 0;
                    }
                }
            }
        }

        // Handle input
        uint8_t b = 0;
        ssize_t n = read(STDIN_FILENO, &b, 1);
        if (n == 1) {
            if (b == 27 || b == 'x' || b == 'X') {
                break;
            }
            S3L_Unit move_speed = S3L_F / 8;
            S3L_Unit rot_speed = S3L_F / 32;

            switch (b) {
                case 'w': case 'W':
                    player_z += move_speed;
                    break;
                case 's': case 'S':
                    player_z -= move_speed;
                    break;
                case 'a': case 'A':
                    player_x -= move_speed;
                    break;
                case 'd': case 'D':
                    player_x += move_speed;
                    break;
                case 'q': case 'Q':
                    player_rot -= rot_speed;
                    break;
                case 'e': case 'E':
                    player_rot += rot_speed;
                    break;
                case 'r': case 'R':
                    player_y += move_speed;
                    break;
                case 'f': case 'F':
                    player_y -= move_speed;
                    break;
            }
        } else if (n < 0 && errno != EAGAIN) {
            break;
        }

        // Update player model position
        models[0].transform.translation.x = player_x;
        models[0].transform.translation.y = player_y;
        models[0].transform.translation.z = player_z;
        models[0].transform.rotation.y = player_rot;

        (void)sys_sleep(16);
    }

    raw_mode_end();
    printf("\nScore: %d/%d\n", score, NUM_CUBES);
    return 0;
}
