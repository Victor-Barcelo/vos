#include <olive.h>

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

static inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

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

int main(int argc, char** argv) {
    uint32_t max_ms = 0;
    if (argc >= 2 && argv && argv[1]) {
        int v = atoi(argv[1]);
        if (v > 0) {
            max_ms = (uint32_t)v;
        }
    }

    if (sys_screen_is_fb() != 1) {
        puts("olivedemo: framebuffer console not available");
        return 1;
    }

    int fb_w = 0;
    int fb_h = 0;
    if (!get_fb_px(&fb_w, &fb_h)) {
        puts("olivedemo: could not query framebuffer size");
        return 1;
    }

    int reserved = reserved_bottom_px();
    if (reserved > 0 && reserved < fb_h) {
        fb_h -= reserved;
    }

    if (fb_w <= 0 || fb_h <= 0) {
        puts("olivedemo: invalid framebuffer size");
        return 1;
    }

    size_t w = (size_t)fb_w;
    size_t h = (size_t)fb_h;

    uint32_t* pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t));
    if (!pixels) {
        printf("olivedemo: malloc failed: %s\n", strerror(errno));
        return 1;
    }

    Olivec_Canvas oc = olivec_canvas(pixels, w, h, w);
    raw_mode_begin();

    const uint32_t bg = rgba(0, 0, 0, 255);
    const uint32_t white = rgba(245, 245, 245, 255);
    const uint32_t blue = rgba(50, 90, 200, 255);
    const uint32_t green = rgba(30, 220, 140, 255);
    const uint32_t magenta = rgba(200, 80, 255, 255);

    uint32_t start_ms = sys_uptime_ms();

    while (1) {
        uint32_t ms = sys_uptime_ms();
        if (max_ms != 0 && (ms - start_ms) >= max_ms) {
            break;
        }

        olivec_fill(oc, bg);

        int cx = (int)(w / 2);
        int cy = (int)(h / 2);

        int box_w = 220;
        int box_h = 80;
        int box_x = (int)((ms / 6) % (uint32_t)(w + box_w)) - box_w;
        int box_y = cy - box_h / 2;
        olivec_rect(oc, box_x, box_y, box_w, box_h, blue);
        olivec_frame(oc, box_x, box_y, box_w, box_h, 2, white);

        int r = 60;
        int circle_x = cx + (int)((ms / 4) % 200) - 100;
        int circle_y = cy + (int)((ms / 7) % 140) - 70;
        olivec_circle(oc, circle_x, circle_y, r, green);
        olivec_frame(oc, circle_x - r, circle_y - r, r * 2, r * 2, 2, white);

        olivec_line(oc, 0, 0, (int)w - 1, (int)h - 1, magenta);
        olivec_line(oc, (int)w - 1, 0, 0, (int)h - 1, magenta);

        const char* msg = "olivedemo (VOS): press 'q' or ESC to quit";
        olivec_text(oc, msg, 12, 12, olivec_default_font, 3, white);

        (void)sys_gfx_blit_rgba(0, 0, (uint32_t)w, (uint32_t)h, pixels);

        uint8_t b = 0;
        ssize_t n = read(STDIN_FILENO, &b, 1);
        if (n == 1 && (b == 27 || b == 'q' || b == 'Q')) {
            break;
        }

        (void)sys_sleep(16);
    }

    raw_mode_end();
    free(pixels);
    return 0;
}
