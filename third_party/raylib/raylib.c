#include "raylib.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "syscall.h"

static bool g_ready = false;
static struct termios g_termios_orig;
static bool g_have_termios = false;

static uint32_t g_start_ms = 0;
static uint32_t g_frame_start_ms = 0;
static float g_last_frame_time = 0.0f;
static uint32_t g_target_frame_ms = 0;

static int g_screen_w = 0;
static int g_screen_h = 0;

static const uint8_t vga16_rgb[16][3] = {
    {0, 0, 0},       // 0 black
    {0, 0, 170},     // 1 blue
    {0, 170, 0},     // 2 green
    {0, 170, 170},   // 3 cyan
    {170, 0, 0},     // 4 red
    {170, 0, 170},   // 5 magenta
    {170, 85, 0},    // 6 brown
    {170, 170, 170}, // 7 light grey
    {85, 85, 85},    // 8 dark grey
    {85, 85, 255},   // 9 light blue
    {85, 255, 85},   // 10 light green
    {85, 255, 255},  // 11 light cyan
    {255, 85, 85},   // 12 light red
    {255, 85, 255},  // 13 light magenta
    {255, 255, 85},  // 14 yellow
    {255, 255, 255}, // 15 white
};

static uint8_t vga16_from_color(Color c) {
    if (c.a == 0) {
        return 0;
    }

    uint32_t best = 0;
    uint32_t best_dist = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < 16; i++) {
        int dr = (int)c.r - (int)vga16_rgb[i][0];
        int dg = (int)c.g - (int)vga16_rgb[i][1];
        int db = (int)c.b - (int)vga16_rgb[i][2];
        uint32_t dist = (uint32_t)(dr * dr + dg * dg + db * db);
        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }
    return (uint8_t)best;
}

static int get_fb_px(int* out_w, int* out_h) {
    if (!out_w || !out_h) {
        return -1;
    }
    *out_w = 0;
    *out_h = 0;

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    if (ioctl(0, TIOCGWINSZ, &ws) != 0) {
        return -1;
    }
    if (ws.ws_xpixel == 0 || ws.ws_ypixel == 0) {
        return -1;
    }
    *out_w = (int)ws.ws_xpixel;
    *out_h = (int)ws.ws_ypixel;
    return 0;
}

static int get_reserved_bottom_px(void) {
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
    // Status bar reserves 1 text row (font height in pixels).
    return (int)info.height;
}

void InitWindow(int width, int height, const char* title) {
    (void)width;
    (void)height;
    (void)title;

    if (g_ready) {
        return;
    }
    if (sys_screen_is_fb() != 1) {
        return;
    }

    if (get_fb_px(&g_screen_w, &g_screen_h) == 0) {
        int reserved = get_reserved_bottom_px();
        if (reserved > 0 && reserved < g_screen_h) {
            g_screen_h -= reserved;
        }
    }

    if (tcgetattr(STDIN_FILENO, &g_termios_orig) == 0) {
        g_have_termios = true;
        struct termios raw = g_termios_orig;
        cfmakeraw(&raw);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    // Hide cursor.
    (void)write(STDOUT_FILENO, "\033[?25l", 6);

    g_start_ms = sys_uptime_ms();
    g_frame_start_ms = g_start_ms;
    g_last_frame_time = 0.0f;
    g_target_frame_ms = 0;
    g_ready = true;
}

bool IsWindowReady(void) {
    return g_ready;
}

void CloseWindow(void) {
    if (!g_ready) {
        return;
    }

    // Show cursor.
    (void)write(STDOUT_FILENO, "\033[?25h", 6);

    if (g_have_termios) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &g_termios_orig);
    }

    g_ready = false;
}

bool WindowShouldClose(void) {
    if (!g_ready) {
        return true;
    }

    uint8_t b = 0;
    ssize_t n = read(STDIN_FILENO, &b, 1);
    if (n == 1) {
        if (b == 27 || b == 'q' || b == 'Q') {
            return true;
        }
    } else if (n < 0 && errno != EAGAIN) {
        // If input is broken, bail out.
        return true;
    }

    return false;
}

int GetScreenWidth(void) {
    return g_screen_w;
}

int GetScreenHeight(void) {
    return g_screen_h;
}

void SetTargetFPS(int fps) {
    if (fps <= 0) {
        g_target_frame_ms = 0;
        return;
    }
    g_target_frame_ms = (uint32_t)(1000u / (uint32_t)fps);
    if (g_target_frame_ms == 0) {
        g_target_frame_ms = 1;
    }
}

float GetFrameTime(void) {
    return g_last_frame_time;
}

double GetTime(void) {
    if (!g_ready) {
        return 0.0;
    }
    uint32_t now = sys_uptime_ms();
    return (double)(now - g_start_ms) / 1000.0;
}

void BeginDrawing(void) {
    if (!g_ready) {
        return;
    }
    g_frame_start_ms = sys_uptime_ms();
}

void EndDrawing(void) {
    if (!g_ready) {
        return;
    }

    uint32_t end_ms = sys_uptime_ms();
    uint32_t elapsed_ms = end_ms - g_frame_start_ms;
    g_last_frame_time = (float)elapsed_ms / 1000.0f;

    if (g_target_frame_ms != 0 && elapsed_ms < g_target_frame_ms) {
        (void)sys_sleep(g_target_frame_ms - elapsed_ms);
    }
}

void ClearBackground(Color color) {
    if (!g_ready) {
        return;
    }
    (void)sys_gfx_clear((uint32_t)vga16_from_color(color));
}

void DrawPixel(int posX, int posY, Color color) {
    if (!g_ready) {
        return;
    }
    (void)sys_gfx_pset(posX, posY, (uint32_t)vga16_from_color(color));
}

void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color) {
    if (!g_ready) {
        return;
    }
    (void)sys_gfx_line(startPosX, startPosY, endPosX, endPosY, (uint32_t)vga16_from_color(color));
}

void DrawRectangle(int posX, int posY, int width, int height, Color color) {
    if (!g_ready) {
        return;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    uint32_t c = (uint32_t)vga16_from_color(color);
    for (int y = 0; y < height; y++) {
        (void)sys_gfx_line(posX, posY + y, posX + width - 1, posY + y, c);
    }
}

void DrawRectangleLines(int posX, int posY, int width, int height, Color color) {
    if (!g_ready) {
        return;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    DrawLine(posX, posY, posX + width - 1, posY, color);
    DrawLine(posX, posY + height - 1, posX + width - 1, posY + height - 1, color);
    DrawLine(posX, posY, posX, posY + height - 1, color);
    DrawLine(posX + width - 1, posY, posX + width - 1, posY + height - 1, color);
}

void DrawText(const char* text, int posX, int posY, int fontSize, Color color) {
    (void)fontSize;
    if (!g_ready || !text) {
        return;
    }

    int idx = sys_font_get_current();
    if (idx < 0) {
        return;
    }
    vos_font_info_t info;
    memset(&info, 0, sizeof(info));
    if (sys_font_info((uint32_t)idx, &info) != 0 || info.width == 0 || info.height == 0) {
        return;
    }

    int col = posX / (int)info.width;
    int row = posY / (int)info.height;
    if (col < 0) col = 0;
    if (row < 0) row = 0;

    uint8_t vga = vga16_from_color(color) & 0x0Fu;
    // Map to ANSI 30-37 + bright, roughly.
    int ansi = 37; // white
    switch (vga) {
        case 0: ansi = 30; break; // black
        case 1: ansi = 34; break; // blue
        case 2: ansi = 32; break; // green
        case 3: ansi = 36; break; // cyan
        case 4: ansi = 31; break; // red
        case 5: ansi = 35; break; // magenta
        case 6: ansi = 33; break; // brown/yellow
        case 7: ansi = 37; break; // light grey
        case 8: ansi = 90; break; // dark grey
        case 9: ansi = 94; break; // light blue
        case 10: ansi = 92; break; // light green
        case 11: ansi = 96; break; // light cyan
        case 12: ansi = 91; break; // light red
        case 13: ansi = 95; break; // light magenta
        case 14: ansi = 93; break; // yellow
        case 15: ansi = 97; break; // white
        default: ansi = 37; break;
    }

    char buf[64];
    int n = snprintf(buf, sizeof(buf), "\033[%d;%dH\033[%dm", row + 1, col + 1, ansi);
    if (n > 0) {
        (void)write(STDOUT_FILENO, buf, (size_t)n);
    }
    (void)write(STDOUT_FILENO, text, strlen(text));
    (void)write(STDOUT_FILENO, "\033[0m", 4);
}
