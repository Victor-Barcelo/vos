#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <termios.h>
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

static int ends_with_ci(const char* s, const char* suffix) {
    if (!s || !suffix) return 0;
    size_t slen = strlen(s);
    size_t tlen = strlen(suffix);
    if (tlen > slen) return 0;
    const char* p = s + (slen - tlen);
    for (size_t i = 0; i < tlen; i++) {
        char a = p[i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static int strcasecmp_ascii(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";
    for (;;) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        if (ca == '\0') {
            return 0;
        }
    }
}

static char* xstrdup(const char* s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char* out = (char*)malloc(n + 1u);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static bool is_supported_image_name(const char* name) {
    if (!name || name[0] == '\0') return false;
    return ends_with_ci(name, ".png") || ends_with_ci(name, ".jpg") || ends_with_ci(name, ".jpeg") || ends_with_ci(name, ".bmp") ||
           ends_with_ci(name, ".tga") || ends_with_ci(name, ".gif") || ends_with_ci(name, ".psd") || ends_with_ci(name, ".pnm") ||
           ends_with_ci(name, ".pgm") || ends_with_ci(name, ".ppm");
}

static const char* path_basename_ptr(const char* path) {
    if (!path) return "";
    const char* slash = strrchr(path, '/');
    return slash ? (slash + 1) : path;
}

static char* path_dirname_dup(const char* path) {
    if (!path || path[0] == '\0') {
        return xstrdup(".");
    }
    const char* slash = strrchr(path, '/');
    if (!slash) {
        return xstrdup(".");
    }
    if (slash == path) {
        return xstrdup("/");
    }
    size_t n = (size_t)(slash - path);
    char* out = (char*)malloc(n + 1u);
    if (!out) {
        return NULL;
    }
    memcpy(out, path, n);
    out[n] = '\0';
    return out;
}

static char* join_path(const char* dir, const char* name) {
    if (!dir || dir[0] == '\0' || strcmp(dir, ".") == 0) {
        return xstrdup(name);
    }
    size_t dlen = strlen(dir);
    bool need_slash = (dlen > 0 && dir[dlen - 1u] != '/');
    size_t nlen = strlen(name);
    size_t out_len = dlen + (need_slash ? 1u : 0u) + nlen;
    char* out = (char*)malloc(out_len + 1u);
    if (!out) {
        return NULL;
    }
    memcpy(out, dir, dlen);
    size_t pos = dlen;
    if (need_slash) {
        out[pos++] = '/';
    }
    memcpy(out + pos, name, nlen);
    out[pos + nlen] = '\0';
    return out;
}

typedef struct img_gallery {
    char* dir;
    char** names;
    size_t count;
    size_t index;
} img_gallery_t;

static int cmp_name_ptrs(const void* a, const void* b) {
    const char* const* pa = (const char* const*)a;
    const char* const* pb = (const char* const*)b;
    return strcasecmp_ascii(*pa, *pb);
}

static void gallery_free(img_gallery_t* g) {
    if (!g) return;
    if (g->names) {
        for (size_t i = 0; i < g->count; i++) {
            free(g->names[i]);
        }
        free(g->names);
    }
    free(g->dir);
    memset(g, 0, sizeof(*g));
}

static int gallery_init(img_gallery_t* g, const char* current_path) {
    if (!g || !current_path) {
        return -1;
    }
    memset(g, 0, sizeof(*g));

    g->dir = path_dirname_dup(current_path);
    if (!g->dir) {
        return -1;
    }

    DIR* d = opendir(g->dir);
    if (!d) {
        return -1;
    }

    size_t cap = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (!e->d_name[0]) continue;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        if (e->d_type == DT_DIR) continue;
        if (!is_supported_image_name(e->d_name)) continue;

        if (g->count == cap) {
            size_t new_cap = cap ? (cap * 2u) : 16u;
            char** new_names = (char**)realloc(g->names, new_cap * sizeof(char*));
            if (!new_names) {
                closedir(d);
                return -1;
            }
            g->names = new_names;
            cap = new_cap;
        }
        g->names[g->count] = xstrdup(e->d_name);
        if (!g->names[g->count]) {
            closedir(d);
            return -1;
        }
        g->count++;
    }
    closedir(d);

    if (g->count == 0) {
        return 0;
    }

    qsort(g->names, g->count, sizeof(char*), cmp_name_ptrs);

    const char* base = path_basename_ptr(current_path);
    for (size_t i = 0; i < g->count; i++) {
        if (strcasecmp_ascii(g->names[i], base) == 0) {
            g->index = i;
            break;
        }
    }
    return 0;
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

static uint8_t* read_entire_file(const char* path, size_t* out_len) {
    if (out_len) *out_len = 0;
    if (!path || path[0] == '\0') {
        return NULL;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return NULL;
    }
    if (sz > 32L * 1024L * 1024L) {
        fclose(f);
        errno = ENOMEM;
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
        free(buf);
        return NULL;
    }
    if (out_len) *out_len = got;
    return buf;
}

static int draw_image_file(const char* path, uint32_t fb_w, uint32_t usable_h) {
    int iw = 0;
    int ih = 0;
    int comp = 0;
    uint8_t* pixels = (uint8_t*)stbi_load(path, &iw, &ih, &comp, 4);
    if (!pixels) {
        printf("img: failed to load '%s'\n", path);
        return -1;
    }
    if (iw <= 0 || ih <= 0) {
        stbi_image_free(pixels);
        puts("img: invalid image dimensions");
        return -1;
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
            return -1;
        }
        out_pixels = (uint8_t*)malloc((size_t)bytes64);
        if (!out_pixels) {
            stbi_image_free(pixels);
            puts("img: out of memory");
            return -1;
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

    if (need_free_out) {
        free(out_pixels);
    }
    stbi_image_free(pixels);
    return 0;
}

int main(int argc, char** argv) {
    bool animate = false;
    const char* path = NULL;
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (!a || a[0] == '\0') continue;
        if (strcmp(a, "-a") == 0 || strcmp(a, "--animate") == 0) {
            animate = true;
            continue;
        }
        path = a;
    }

    if (!path) {
        puts("Usage: img <file>");
        puts("       img -a <file.gif>   # animate GIF (Ctrl+C to stop)");
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

    if (animate && ends_with_ci(path, ".gif")) {
        size_t file_len = 0;
        uint8_t* file = read_entire_file(path, &file_len);
        if (!file) {
            printf("img: failed to read '%s'\n", path);
            return 1;
        }

        int iw = 0;
        int ih = 0;
        int frames = 0;
        int comp = 0;
        int* delays = NULL;
        uint8_t* all = (uint8_t*)stbi_load_gif_from_memory(file, (int)file_len, &delays, &iw, &ih, &frames, &comp, 4);
        const char* stbi_err = all ? NULL : stbi_failure_reason();
        free(file);

        // NOTE: stb_image frees internal allocations on failure, but it does not
        // reliably clear output pointers (e.g. `delays`) in all out-of-memory
        // paths. Avoid freeing `delays` unless decoding succeeds.
        if (!all) {
            printf("img: failed to decode gif '%s'%s%s\n", path, stbi_err ? ": " : "", stbi_err ? stbi_err : "");
            return 1;
        }
        if (iw <= 0 || ih <= 0 || frames <= 0) {
            if (delays) free(delays);
            stbi_image_free(all);
            printf("img: invalid gif '%s'\n", path);
            return 1;
        }

        uint32_t in_w = (uint32_t)iw;
        uint32_t in_h = (uint32_t)ih;

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

        uint8_t* scaled = NULL;
        if (out_w != in_w || out_h != in_h) {
            uint64_t bytes64 = (uint64_t)out_w * (uint64_t)out_h * 4u;
            if (bytes64 > 64u * 1024u * 1024u) {
                if (delays) free(delays);
                stbi_image_free(all);
                puts("img: scaled gif too large");
                return 1;
            }
            scaled = (uint8_t*)malloc((size_t)bytes64);
            if (!scaled) {
                if (delays) free(delays);
                stbi_image_free(all);
                puts("img: out of memory");
                return 1;
            }
        }

        (void)sys_gfx_clear(0);

        int32_t x0 = (int32_t)((fb_w - out_w) / 2u);
        int32_t y0 = (int32_t)((usable_h - out_h) / 2u);

        uint32_t frame_stride = in_w * in_h * 4u;
        puts("Animating GIF. Press Ctrl+C to stop.");
        for (;;) {
            for (int fi = 0; fi < frames; fi++) {
                const uint8_t* frame = all + (uint32_t)fi * frame_stride;
                const uint8_t* src = frame;
                if (scaled) {
                    nearest_scale_rgba(scaled, out_w, out_h, frame, in_w, in_h);
                    src = scaled;
                }
                (void)sys_gfx_blit_rgba(x0, y0, out_w, out_h, src);
                int d = delays ? delays[fi] : 100;
                if (d <= 0) d = 100;
                if (d > 5000) d = 5000;
                (void)sys_sleep((uint32_t)d);
            }
        }
    }

    img_gallery_t gallery;
    int gal_rc = gallery_init(&gallery, path);
    if (gal_rc != 0) {
        memset(&gallery, 0, sizeof(gallery));
    }

    char* current_path = xstrdup(path);
    if (!current_path) {
        gallery_free(&gallery);
        puts("img: out of memory");
        return 1;
    }

    if (draw_image_file(current_path, fb_w, usable_h) != 0) {
        free(current_path);
        gallery_free(&gallery);
        return 1;
    }

    struct termios old_termios;
    bool have_termios = (tcgetattr(0, &old_termios) == 0);
    if (have_termios) {
        struct termios t = old_termios;
        t.c_lflag &= (tcflag_t) ~(ECHO | ICANON);
        if (VMIN >= 0 && VMIN < NCCS) t.c_cc[VMIN] = 1;
        if (VTIME >= 0 && VTIME < NCCS) t.c_cc[VTIME] = 0;
        (void)tcsetattr(0, TCSANOW, &t);
    }

    if (gallery.count >= 2) {
        puts("Left/Right to browse, 'q' to quit.");
    } else {
        puts("Press 'q' to quit.");
    }

    enum { KEY_NORMAL, KEY_ESC, KEY_CSI } state = KEY_NORMAL;
    for (;;) {
        char c = 0;
        int n = (int)read(0, &c, 1);
        if (n <= 0) {
            break;
        }

        if (state == KEY_NORMAL) {
            if (c == 'q' || c == 'Q') {
                break;
            }
            if ((unsigned char)c == 0x1B) { // ESC
                state = KEY_ESC;
                continue;
            }
        } else if (state == KEY_ESC) {
            if (c == '[') {
                state = KEY_CSI;
                continue;
            }
            state = KEY_NORMAL;
            continue;
        } else if (state == KEY_CSI) {
            bool next = false;
            bool prev = false;
            if (c == 'C') next = true;  // Right
            if (c == 'D') prev = true;  // Left
            state = KEY_NORMAL;

            if ((next || prev) && gallery.count >= 2) {
                if (next) {
                    gallery.index = (gallery.index + 1u) % gallery.count;
                } else {
                    gallery.index = (gallery.index + gallery.count - 1u) % gallery.count;
                }
                char* new_path = join_path(gallery.dir ? gallery.dir : ".", gallery.names[gallery.index]);
                if (new_path) {
                    if (draw_image_file(new_path, fb_w, usable_h) == 0) {
                        free(current_path);
                        current_path = new_path;
                        puts("Left/Right to browse, 'q' to quit.");
                    } else {
                        free(new_path);
                    }
                }
            }
            continue;
        }
    }

    // Clear back to a clean prompt.
    fputs("\x1b[2J\x1b[H", stdout);

    if (have_termios) {
        (void)tcsetattr(0, TCSANOW, &old_termios);
    }
    free(current_path);
    gallery_free(&gallery);
    return 0;
}
