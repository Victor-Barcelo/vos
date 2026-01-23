#include "screen.h"
#include "io.h"
#include "serial.h"
#include "string.h"
#include "multiboot.h"
#include "font.h"
#include "font_terminus_psf2.h"
#include "font_vga_psf2.h"
#include "statusbar.h"
#include "kerrno.h"

typedef enum {
    SCREEN_BACKEND_VGA_TEXT = 0,
    SCREEN_BACKEND_FRAMEBUFFER = 1,
} screen_backend_t;

// VGA text buffer address
static uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;

static screen_backend_t backend = SCREEN_BACKEND_VGA_TEXT;

static int screen_cols_value = VGA_WIDTH;
static int screen_rows_value = VGA_HEIGHT;

// Current cursor position (in text cells)
static int cursor_x = 0;
static int cursor_y = 0;

// Cursor rendering state
static bool cursor_enabled = true;
static bool cursor_vt_hidden = false;
static int cursor_drawn_x = -1;
static int cursor_drawn_y = -1;

// Mouse cursor overlay (framebuffer text console only).
static bool mouse_cursor_enabled = false;
static int mouse_cursor_x = 0;
static int mouse_cursor_y = 0;
static int mouse_drawn_x = -1;
static int mouse_drawn_y = -1;

// Canonical console colors as xterm palette indices (0-255).
// In VGA text mode we map these down to the nearest VGA 16-color entry.
static uint8_t current_fg = VGA_WHITE;
static uint8_t current_bg = VGA_BLUE;
static uint8_t default_fg = VGA_WHITE;
static uint8_t default_bg = VGA_BLUE;

// Current VGA text attribute (derived from current_fg/current_bg).
static uint8_t current_color = 0x0F;

static int reserved_bottom_rows = 0;

// Minimal ANSI/VT100 parsing (CSI sequences) used by some vendored CLI code.
typedef enum {
    ANSI_STATE_NONE = 0,
    ANSI_STATE_ESC,
    ANSI_STATE_CSI,
} ansi_state_t;

static ansi_state_t ansi_state = ANSI_STATE_NONE;
static int ansi_params[32];
static int ansi_param_count = 0;
static int ansi_current = -1;
static bool ansi_private = false;
static int ansi_saved_x = 0;
static int ansi_saved_y = 0;
static int ansi_scroll_top = 0;
static int ansi_scroll_bottom = 0;
// Xterm mouse tracking modes enabled via CSI ?1000/?1002/?1003 h/l.
// We track them individually so the wheel can remain available for scrollback
// when only basic click reporting is enabled (e.g. shell prompt).
enum {
    VT_MOUSE_MODE_1000 = 1u << 0,
    VT_MOUSE_MODE_1002 = 1u << 1,
    VT_MOUSE_MODE_1003 = 1u << 2,
};
static uint8_t vt_mouse_mode_mask = 0;
static bool vt_mouse_sgr = false;

static inline int screen_phys_x(int x);
static inline int screen_phys_y(int y);
static void fb_render_cell(int x, int y);
static void fb_update_mouse_cursor(void);
static uint32_t fb_color_from_vga(uint8_t idx);
static uint32_t fb_color_from_xterm(uint8_t idx);
static void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pixel);
static void update_cursor(void);
static void vga_scroll(void);
static void fb_scroll(void);
static void vga_scroll_down(void);
static void fb_scroll_down(void);

// "Safe area" padding (in character cells).
static int pad_left_cols = 0;
static int pad_right_cols = 0;
static int pad_top_rows = 0;
static int pad_bottom_rows = 0;

// Framebuffer pixel origin for the top-left text cell (0,0).
static uint32_t fb_origin_x = 0;
static uint32_t fb_origin_y = 0;

// Framebuffer mode state
#define FB_MAX_COLS 200
#define FB_MAX_ROWS 100

static uint8_t* fb_addr = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint8_t fb_bpp = 0;
static uint8_t fb_bytes_per_pixel = 0;
static uint8_t fb_type = 0;
static uint8_t fb_r_pos = 0;
static uint8_t fb_r_size = 0;
static uint8_t fb_g_pos = 0;
static uint8_t fb_g_size = 0;
static uint8_t fb_b_pos = 0;
static uint8_t fb_b_size = 0;
static font_t fb_font;
static int fb_font_current_index = -1;

// Extra fonts embedded as binary objects by the build.
extern const uint8_t _binary_third_party_fonts_spleen_spleen_12x24_psf_start[];
extern const uint8_t _binary_third_party_fonts_spleen_spleen_12x24_psf_end[];
extern const uint8_t _binary_third_party_fonts_spleen_spleen_16x32_psf_start[];
extern const uint8_t _binary_third_party_fonts_spleen_spleen_16x32_psf_end[];
extern const uint8_t _binary_third_party_fonts_spleen_spleen_32x64_psf_start[];
extern const uint8_t _binary_third_party_fonts_spleen_spleen_32x64_psf_end[];
extern const uint8_t _binary_third_party_fonts_terminus_Uni3_Terminus28x14_psf_start[];
extern const uint8_t _binary_third_party_fonts_terminus_Uni3_Terminus28x14_psf_end[];
extern const uint8_t _binary_third_party_fonts_terminus_Uni3_TerminusBold32x16_psf_start[];
extern const uint8_t _binary_third_party_fonts_terminus_Uni3_TerminusBold32x16_psf_end[];

typedef struct {
    const char* name;
    const uint8_t* data;
    const uint8_t* end;
    uint32_t len;
} fb_font_source_t;

static uint32_t fb_blob_len(const uint8_t* start, const uint8_t* end) {
    if (!start || !end) {
        return 0;
    }
    if (end < start) {
        return 0;
    }
    return (uint32_t)(end - start);
}

enum {
    FB_FONT_VGA_28X16 = 0,
    FB_FONT_VGA_32X16 = 1,
    FB_FONT_TERMINUS_24X12 = 2,
    FB_FONT_TERMINUS_32X16 = 3,
    FB_FONT_TERMINUS_28X14 = 4,
    FB_FONT_TERMINUS_BOLD_32X16 = 5,
    FB_FONT_SPLEEN_12X24 = 6,
    FB_FONT_SPLEEN_16X32 = 7,
    FB_FONT_SPLEEN_32X64 = 8,
};

static const fb_font_source_t fb_fonts[] = {
    [FB_FONT_VGA_28X16] = {"vga-28x16", font_vga28x16_psf2, NULL, 0},
    [FB_FONT_VGA_32X16] = {"vga-32x16", font_vga32x16_psf2, NULL, 0},
    [FB_FONT_TERMINUS_24X12] = {"terminus-24x12", font_terminus24x12_psf2, NULL, 0},
    [FB_FONT_TERMINUS_32X16] = {"terminus-32x16", font_terminus32x16_psf2, NULL, 0},
    [FB_FONT_TERMINUS_28X14] = {"terminus-28x14", _binary_third_party_fonts_terminus_Uni3_Terminus28x14_psf_start,
                                _binary_third_party_fonts_terminus_Uni3_Terminus28x14_psf_end, 0},
    [FB_FONT_TERMINUS_BOLD_32X16] = {"terminus-bold-32x16", _binary_third_party_fonts_terminus_Uni3_TerminusBold32x16_psf_start,
                                     _binary_third_party_fonts_terminus_Uni3_TerminusBold32x16_psf_end, 0},
    [FB_FONT_SPLEEN_12X24] = {"spleen-12x24", _binary_third_party_fonts_spleen_spleen_12x24_psf_start,
                              _binary_third_party_fonts_spleen_spleen_12x24_psf_end, 0},
    [FB_FONT_SPLEEN_16X32] = {"spleen-16x32", _binary_third_party_fonts_spleen_spleen_16x32_psf_start,
                              _binary_third_party_fonts_spleen_spleen_16x32_psf_end, 0},
    [FB_FONT_SPLEEN_32X64] = {"spleen-32x64", _binary_third_party_fonts_spleen_spleen_32x64_psf_start,
                              _binary_third_party_fonts_spleen_spleen_32x64_psf_end, 0},
};

static int fb_font_count_value(void) {
    return (int)(sizeof(fb_fonts) / sizeof(fb_fonts[0]));
}

static bool fb_font_source_get(int index, const fb_font_source_t** out_src, const uint8_t** out_data, uint32_t* out_len) {
    if (out_src) {
        *out_src = NULL;
    }
    if (out_data) {
        *out_data = NULL;
    }
    if (out_len) {
        *out_len = 0;
    }
    int count = fb_font_count_value();
    if (index < 0 || index >= count) {
        return false;
    }

    const fb_font_source_t* src = &fb_fonts[index];
    if (!src->data) {
        return false;
    }

    uint32_t len = src->len;
    if (len == 0) {
        if (src->end) {
            len = fb_blob_len(src->data, src->end);
        } else {
            switch (index) {
                case FB_FONT_VGA_28X16:
                    len = font_vga28x16_psf2_len;
                    break;
                case FB_FONT_VGA_32X16:
                    len = font_vga32x16_psf2_len;
                    break;
                case FB_FONT_TERMINUS_24X12:
                    len = font_terminus24x12_psf2_len;
                    break;
                case FB_FONT_TERMINUS_32X16:
                    len = font_terminus32x16_psf2_len;
                    break;
                default:
                    break;
            }
        }
    }
    if (len == 0) {
        return false;
    }

    if (out_src) {
        *out_src = src;
    }
    if (out_data) {
        *out_data = src->data;
    }
    if (out_len) {
        *out_len = len;
    }
    return true;
}

typedef uint32_t fb_cell_t;

static inline fb_cell_t fb_cell_make(uint8_t ch, uint8_t fg, uint8_t bg) {
    return (fb_cell_t)ch | ((fb_cell_t)fg << 8) | ((fb_cell_t)bg << 16);
}

static inline uint8_t fb_cell_ch(fb_cell_t cell) {
    return (uint8_t)(cell & 0xFFu);
}

static inline uint8_t fb_cell_fg(fb_cell_t cell) {
    return (uint8_t)((cell >> 8) & 0xFFu);
}

static inline uint8_t fb_cell_bg(fb_cell_t cell) {
    return (uint8_t)((cell >> 16) & 0xFFu);
}

static fb_cell_t fb_cells[FB_MAX_COLS * FB_MAX_ROWS];

// UTF-8 decoding for framebuffer text console (so userland can print Unicode).
typedef struct {
    uint32_t codepoint;
    uint32_t min;
    uint8_t remaining;
} utf8_state_t;

static utf8_state_t utf8_state;

#define FB_UNICODE_MAP_MAX 2048u
typedef struct {
    uint32_t codepoint;
    uint8_t glyph;
} fb_unicode_map_entry_t;

static fb_unicode_map_entry_t fb_unicode_map[FB_UNICODE_MAP_MAX];
static uint32_t fb_unicode_map_count = 0;
static uint8_t fb_unicode_replacement_glyph = (uint8_t)'?';
static bool fb_unicode_ready = false;

#define SCROLLBACK_MAX_LINES 1024
static fb_cell_t scrollback_cells[SCROLLBACK_MAX_LINES * FB_MAX_COLS];
static uint32_t scrollback_head = 0;
static uint32_t scrollback_count = 0;
static uint32_t scrollback_view_offset = 0;
static int scrollback_cols = 0;
static bool cursor_force_hidden = false;

static const uint8_t vga_palette_rgb[16][3] = {
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

static const uint8_t xterm16_to_vga[16] = {
    VGA_BLACK,         // 0 black
    VGA_RED,           // 1 red
    VGA_GREEN,         // 2 green
    VGA_BROWN,         // 3 yellow (dim)
    VGA_BLUE,          // 4 blue
    VGA_MAGENTA,       // 5 magenta
    VGA_CYAN,          // 6 cyan
    VGA_LIGHT_GREY,    // 7 white (dim)
    VGA_DARK_GREY,     // 8 bright black
    VGA_LIGHT_RED,     // 9 bright red
    VGA_LIGHT_GREEN,   // 10 bright green
    VGA_YELLOW,        // 11 bright yellow
    VGA_LIGHT_BLUE,    // 12 bright blue
    VGA_LIGHT_MAGENTA, // 13 bright magenta
    VGA_LIGHT_CYAN,    // 14 bright cyan
    VGA_WHITE,         // 15 bright white
};

static const uint8_t vga16_to_xterm[16] = {
    0,  // VGA_BLACK
    4,  // VGA_BLUE
    2,  // VGA_GREEN
    6,  // VGA_CYAN
    1,  // VGA_RED
    5,  // VGA_MAGENTA
    3,  // VGA_BROWN
    7,  // VGA_LIGHT_GREY
    8,  // VGA_DARK_GREY
    12, // VGA_LIGHT_BLUE
    10, // VGA_LIGHT_GREEN
    14, // VGA_LIGHT_CYAN
    9,  // VGA_LIGHT_RED
    13, // VGA_LIGHT_MAGENTA
    11, // VGA_YELLOW
    15, // VGA_WHITE
};

static uint8_t xterm_from_vga_index(uint8_t vga_idx) {
    return vga16_to_xterm[vga_idx & 0x0Fu];
}

static void xterm_rgb_from_index(uint8_t idx, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!r || !g || !b) {
        return;
    }

    if (idx < 16u) {
        uint8_t vga = xterm16_to_vga[idx];
        *r = vga_palette_rgb[vga][0];
        *g = vga_palette_rgb[vga][1];
        *b = vga_palette_rgb[vga][2];
        return;
    }

    if (idx >= 16u && idx <= 231u) {
        static const uint8_t levels[6] = {0, 95, 135, 175, 215, 255};
        uint8_t i = (uint8_t)(idx - 16u);
        uint8_t rr = (uint8_t)(i / 36u);
        uint8_t gg = (uint8_t)((i / 6u) % 6u);
        uint8_t bb = (uint8_t)(i % 6u);
        *r = levels[rr];
        *g = levels[gg];
        *b = levels[bb];
        return;
    }

    // 232-255 grayscale ramp.
    uint8_t shade = (uint8_t)(8u + (uint8_t)(idx - 232u) * 10u);
    *r = shade;
    *g = shade;
    *b = shade;
}

static uint8_t vga_from_xterm_index(uint8_t idx) {
    if (idx < 16u) {
        return xterm16_to_vga[idx];
    }

    uint8_t r = 0, g = 0, b = 0;
    xterm_rgb_from_index(idx, &r, &g, &b);

    uint32_t best = 0;
    uint32_t best_dist = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < 16u; i++) {
        int dr = (int)r - (int)vga_palette_rgb[i][0];
        int dg = (int)g - (int)vga_palette_rgb[i][1];
        int db = (int)b - (int)vga_palette_rgb[i][2];
        uint32_t dist = (uint32_t)(dr * dr + dg * dg + db * db);
        if (dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }
    return (uint8_t)best;
}

static uint8_t xterm_level_6(uint8_t v) {
    if (v < 48u) return 0u;
    if (v < 115u) return 1u;
    uint32_t t = (uint32_t)(v - 35u);
    uint32_t q = t / 40u;
    if (q > 5u) q = 5u;
    return (uint8_t)q;
}

static uint8_t xterm_index_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    if (r == g && g == b) {
        if (r < 8u) {
            return 0u;
        }
        if (r > 238u) {
            return 15u;
        }
        uint32_t step = (uint32_t)(r - 8u);
        uint32_t idx = (step + 5u) / 10u;
        if (idx > 23u) idx = 23u;
        return (uint8_t)(232u + idx);
    }

    // Approximate to the 6x6x6 color cube.
    uint8_t rr = xterm_level_6(r);
    uint8_t gg = xterm_level_6(g);
    uint8_t bb = xterm_level_6(b);
    return (uint8_t)(16u + 36u * rr + 6u * gg + bb);
}

static int usable_height(void) {
    int h = screen_rows_value - reserved_bottom_rows;
    if (h < 1) {
        h = 1;
    }
    return h;
}

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static inline uint8_t vga_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

static void update_vga_colors(void) {
    uint8_t fg = vga_from_xterm_index(current_fg);
    uint8_t bg = vga_from_xterm_index(current_bg);
    current_color = vga_color(fg, bg);
}

static void scrollback_reset(void) {
    scrollback_head = 0;
    scrollback_count = 0;
    scrollback_view_offset = 0;
    scrollback_cols = screen_cols_value;
    cursor_force_hidden = false;
}

static uint32_t utf8_decode_one(const uint8_t* bytes, uint32_t len, bool* ok) {
    if (ok) {
        *ok = false;
    }
    if (!bytes || len == 0) {
        return 0;
    }

    uint8_t b0 = bytes[0];
    if (b0 < 0x80u) {
        if (ok) *ok = true;
        return (uint32_t)b0;
    }
    if (b0 >= 0xC2u && b0 <= 0xDFu) {
        if (len < 2) return 0;
        uint8_t b1 = bytes[1];
        if ((b1 & 0xC0u) != 0x80u) return 0;
        uint32_t cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
        if (cp < 0x80u) return 0;
        if (ok) *ok = true;
        return cp;
    }
    if (b0 >= 0xE0u && b0 <= 0xEFu) {
        if (len < 3) return 0;
        uint8_t b1 = bytes[1];
        uint8_t b2 = bytes[2];
        if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u) return 0;
        uint32_t cp = ((uint32_t)(b0 & 0x0Fu) << 12) | ((uint32_t)(b1 & 0x3Fu) << 6) | (uint32_t)(b2 & 0x3Fu);
        if (cp < 0x800u) return 0;
        if (cp >= 0xD800u && cp <= 0xDFFFu) return 0;
        if (ok) *ok = true;
        return cp;
    }
    if (b0 >= 0xF0u && b0 <= 0xF4u) {
        if (len < 4) return 0;
        uint8_t b1 = bytes[1];
        uint8_t b2 = bytes[2];
        uint8_t b3 = bytes[3];
        if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u || (b3 & 0xC0u) != 0x80u) return 0;
        uint32_t cp = ((uint32_t)(b0 & 0x07u) << 18) | ((uint32_t)(b1 & 0x3Fu) << 12) | ((uint32_t)(b2 & 0x3Fu) << 6) | (uint32_t)(b3 & 0x3Fu);
        if (cp < 0x10000u || cp > 0x10FFFFu) return 0;
        if (ok) *ok = true;
        return cp;
    }
    return 0;
}

static void fb_unicode_map_sort(void) {
    // Insertion sort: the map is small (hundreds of entries).
    for (uint32_t i = 1; i < fb_unicode_map_count; i++) {
        fb_unicode_map_entry_t key = fb_unicode_map[i];
        uint32_t j = i;
        while (j > 0 && fb_unicode_map[j - 1u].codepoint > key.codepoint) {
            fb_unicode_map[j] = fb_unicode_map[j - 1u];
            j--;
        }
        fb_unicode_map[j] = key;
    }
}

static bool fb_unicode_lookup(uint32_t codepoint, uint8_t* out_glyph) {
    if (!fb_unicode_ready || fb_unicode_map_count == 0 || !out_glyph) {
        return false;
    }

    uint32_t lo = 0;
    uint32_t hi = fb_unicode_map_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        uint32_t cp = fb_unicode_map[mid].codepoint;
        if (cp == codepoint) {
            *out_glyph = fb_unicode_map[mid].glyph;
            return true;
        }
        if (cp < codepoint) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return false;
}

static void fb_unicode_build_map(void) {
    fb_unicode_map_count = 0;
    fb_unicode_ready = false;
    fb_unicode_replacement_glyph = (uint8_t)'?';

    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return;
    }
    if (!fb_font.data || fb_font.data_len < fb_font.headersize) {
        return;
    }
    if ((fb_font.flags & 0x1u) == 0) {
        return; // no Unicode table
    }

    uint32_t glyph_bytes = fb_font.glyph_count * fb_font.bytes_per_glyph;
    uint32_t table_off = fb_font.headersize + glyph_bytes;
    if (table_off < fb_font.headersize || table_off > fb_font.data_len) {
        return;
    }

    const uint8_t* p = fb_font.data + table_off;
    uint32_t remaining = fb_font.data_len - table_off;

    uint32_t glyph = 0;
    uint8_t seq[8];
    uint32_t seq_len = 0;

    while (glyph < fb_font.glyph_count && remaining > 0) {
        uint8_t b = *p++;
        remaining--;

        if (b == 0xFFu) {
            // End of this glyph's entries.
            seq_len = 0;
            glyph++;
            continue;
        }

        if (b == 0xFEu) {
            // Sequence separator (used for multi-codepoint sequences). Ignore these.
            seq_len = 0;
            continue;
        }

        if (seq_len < (uint32_t)sizeof(seq)) {
            seq[seq_len++] = b;
        } else {
            // Too long/invalid: drop this entry.
            seq_len = 0;
        }

        bool ok = false;
        uint32_t cp = utf8_decode_one(seq, seq_len, &ok);
        if (!ok) {
            continue;
        }
        seq_len = 0;

        if (cp == 0) {
            continue;
        }
        if (glyph > 0xFFu) {
            // Our cell storage is 8-bit. Ignore glyphs beyond 255 for now.
            continue;
        }

        bool exists = false;
        for (uint32_t i = 0; i < fb_unicode_map_count; i++) {
            if (fb_unicode_map[i].codepoint == cp) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        if (fb_unicode_map_count < FB_UNICODE_MAP_MAX) {
            fb_unicode_map[fb_unicode_map_count++] = (fb_unicode_map_entry_t){
                .codepoint = cp,
                .glyph = (uint8_t)glyph,
            };
        }
    }

    if (fb_unicode_map_count == 0) {
        return;
    }

    fb_unicode_map_sort();
    fb_unicode_ready = true;

    uint8_t repl = 0;
    if (fb_unicode_lookup(0xFFFDu, &repl)) {
        fb_unicode_replacement_glyph = repl;
    } else {
        fb_unicode_replacement_glyph = (uint8_t)'?';
    }
}

static fb_cell_t* scrollback_line_ptr(uint32_t idx) {
    uint32_t real = (scrollback_head + idx) % SCROLLBACK_MAX_LINES;
    return &scrollback_cells[real * FB_MAX_COLS];
}

static void scrollback_push_line(const fb_cell_t* line, int cols) {
    if (!line) {
        return;
    }
    if (cols < 1) {
        return;
    }
    if (cols > FB_MAX_COLS) {
        cols = FB_MAX_COLS;
    }

    if (scrollback_cols != cols) {
        scrollback_reset();
        scrollback_cols = cols;
    }

    fb_cell_t* dst = 0;
    if (scrollback_count < SCROLLBACK_MAX_LINES) {
        dst = scrollback_line_ptr(scrollback_count);
        scrollback_count++;
    } else {
        dst = &scrollback_cells[scrollback_head * FB_MAX_COLS];
        scrollback_head = (scrollback_head + 1) % SCROLLBACK_MAX_LINES;
    }

    memcpy(dst, line, (size_t)cols * sizeof(fb_cell_t));
    if (cols < FB_MAX_COLS) {
        fb_cell_t blank = fb_cell_make((uint8_t)' ', current_fg, current_bg);
        for (int x = cols; x < FB_MAX_COLS; x++) {
            dst[x] = blank;
        }
    }

    if (scrollback_view_offset > 0 && scrollback_view_offset < scrollback_count) {
        scrollback_view_offset++;
    }
}

static void cursor_clamp(void) {
    int max_y = usable_height() - 1;
    if (max_y < 0) max_y = 0;

    if (cursor_x < 0) cursor_x = 0;
    if (cursor_y < 0) cursor_y = 0;
    if (cursor_x >= screen_cols_value) cursor_x = screen_cols_value - 1;
    if (cursor_y > max_y) cursor_y = max_y;
}

static void ansi_scroll_region_reset(void) {
    ansi_scroll_top = 0;
    ansi_scroll_bottom = usable_height() - 1;
    if (ansi_scroll_bottom < 0) {
        ansi_scroll_bottom = 0;
    }
}

static void ansi_scroll_region_clamp(void) {
    int height = usable_height();
    if (height < 1) {
        height = 1;
    }
    if (ansi_scroll_top < 0) {
        ansi_scroll_top = 0;
    }
    if (ansi_scroll_bottom < 0) {
        ansi_scroll_bottom = height - 1;
    }
    if (ansi_scroll_top >= height) {
        ansi_scroll_top = 0;
    }
    if (ansi_scroll_bottom >= height) {
        ansi_scroll_bottom = height - 1;
    }
    if (ansi_scroll_top > ansi_scroll_bottom) {
        ansi_scroll_top = 0;
        ansi_scroll_bottom = height - 1;
    }
}

static void ansi_erase_to_eol(void) {
    int y = cursor_y;
    if (y < 0 || y >= usable_height()) {
        return;
    }
    if (cursor_x < 0) {
        cursor_x = 0;
    }
    if (cursor_x >= screen_cols_value) {
        return;
    }

    bool render_now = !(backend == SCREEN_BACKEND_FRAMEBUFFER && scrollback_view_offset > 0);

    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        fb_cell_t blank = fb_cell_make((uint8_t)' ', current_fg, current_bg);
        fb_cell_t* row = &fb_cells[y * screen_cols_value];
        for (int x = cursor_x; x < screen_cols_value; x++) {
            row[x] = blank;
            if (render_now) {
                fb_render_cell(x, y);
            }
        }
    } else {
        for (int x = cursor_x; x < screen_cols_value; x++) {
            VGA_BUFFER[screen_phys_y(y) * VGA_WIDTH + screen_phys_x(x)] = vga_entry(' ', current_color);
        }
    }
}

static void ansi_erase_line(int mode) {
    int y = cursor_y;
    if (y < 0 || y >= usable_height()) {
        return;
    }
    int x0 = 0;
    int x1 = screen_cols_value - 1;
    if (x1 < 0) {
        return;
    }

    if (mode == 0) {
        x0 = cursor_x;
    } else if (mode == 1) {
        x1 = cursor_x;
    } else if (mode == 2) {
        x0 = 0;
        x1 = screen_cols_value - 1;
    } else {
        return;
    }

    if (x0 < 0) x0 = 0;
    if (x1 < 0) x1 = 0;
    if (x0 >= screen_cols_value) return;
    if (x1 >= screen_cols_value) x1 = screen_cols_value - 1;
    if (x0 > x1) return;

    bool render_now = !(backend == SCREEN_BACKEND_FRAMEBUFFER && scrollback_view_offset > 0);

    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        fb_cell_t blank = fb_cell_make((uint8_t)' ', current_fg, current_bg);
        fb_cell_t* row = &fb_cells[y * screen_cols_value];
        for (int x = x0; x <= x1; x++) {
            row[x] = blank;
            if (render_now) {
                fb_render_cell(x, y);
            }
        }
    } else {
        for (int x = x0; x <= x1; x++) {
            VGA_BUFFER[screen_phys_y(y) * VGA_WIDTH + screen_phys_x(x)] = vga_entry(' ', current_color);
        }
    }
}

static void ansi_erase_display(int mode) {
    int height = usable_height();
    if (height < 1) {
        height = 1;
    }

    if (mode == 2) {
        screen_clear();
        // Keep the status bar visible after a full-screen clear (common in
        // userland tools that rely on ANSI sequences).
        statusbar_refresh();
        return;
    }

    if (mode == 0) {
        // Cursor to end of screen.
        ansi_erase_line(0);
        for (int y = cursor_y + 1; y < height; y++) {
            int saved_y = cursor_y;
            int saved_x = cursor_x;
            cursor_y = y;
            cursor_x = 0;
            ansi_erase_line(2);
            cursor_y = saved_y;
            cursor_x = saved_x;
        }
        return;
    }

    if (mode == 1) {
        // Start of screen to cursor.
        for (int y = 0; y < cursor_y; y++) {
            int saved_y = cursor_y;
            int saved_x = cursor_x;
            cursor_y = y;
            cursor_x = 0;
            ansi_erase_line(2);
            cursor_y = saved_y;
            cursor_x = saved_x;
        }
        ansi_erase_line(1);
        return;
    }
}

static void ansi_insert_lines(int n) {
    if (n < 1) {
        n = 1;
    }

    ansi_scroll_region_clamp();
    if (cursor_y < ansi_scroll_top || cursor_y > ansi_scroll_bottom) {
        return;
    }

    int top = cursor_y;
    int bottom = ansi_scroll_bottom;
    int region_rows = bottom - top + 1;
    if (region_rows <= 0) {
        return;
    }
    if (n > region_rows) {
        n = region_rows;
    }

    bool render_now = !(backend == SCREEN_BACKEND_FRAMEBUFFER && scrollback_view_offset > 0);

    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        // Undraw cursor overlay before copying pixel rows.
        if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
            fb_render_cell(cursor_drawn_x, cursor_drawn_y);
            cursor_drawn_x = -1;
            cursor_drawn_y = -1;
        }
        if (mouse_cursor_enabled && mouse_drawn_x >= 0 && mouse_drawn_y >= 0) {
            fb_render_cell(mouse_drawn_x, mouse_drawn_y);
        }

        int cols = screen_cols_value;
        size_t row_bytes = (size_t)cols * sizeof(fb_cell_t);
        int move_rows = region_rows - n;
        if (move_rows > 0) {
            memmove(&fb_cells[(top + n) * cols], &fb_cells[top * cols], row_bytes * (size_t)move_rows);
        }

        fb_cell_t blank = fb_cell_make((uint8_t)' ', current_fg, current_bg);
        for (int y = 0; y < n; y++) {
            fb_cell_t* row = &fb_cells[(top + y) * cols];
            for (int x = 0; x < cols; x++) {
                row[x] = blank;
            }
        }

        if (render_now) {
            uint32_t region_y = fb_origin_y + (uint32_t)top * fb_font.height;
            uint32_t shift_px = (uint32_t)n * fb_font.height;
            uint32_t region_px_height = (uint32_t)region_rows * fb_font.height;
            if (shift_px >= region_px_height) {
                shift_px = region_px_height;
            }
            uint32_t copy_bytes = (region_px_height - shift_px) * fb_pitch;

            uint8_t* src = fb_addr + region_y * fb_pitch;
            uint8_t* dst = src + shift_px * fb_pitch;
            memmove(dst, src, copy_bytes);

            uint32_t bg_px = fb_color_from_xterm(current_bg);
            fb_fill_rect(0, region_y, fb_width, shift_px, bg_px);
        }

        if (mouse_cursor_enabled) {
            fb_update_mouse_cursor();
        }
    } else {
        // Shift physical rows down by N within the scroll region.
        for (int y = bottom; y >= top + n; y--) {
            int dst_y = screen_phys_y(y);
            int src_y = screen_phys_y(y - n);
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_BUFFER[dst_y * VGA_WIDTH + x] = VGA_BUFFER[src_y * VGA_WIDTH + x];
            }
        }

        for (int y = top; y < top + n; y++) {
            int phys_y = screen_phys_y(y);
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_BUFFER[phys_y * VGA_WIDTH + x] = vga_entry(' ', current_color);
            }
        }
    }
}

static void ansi_delete_lines(int n) {
    if (n < 1) {
        n = 1;
    }

    ansi_scroll_region_clamp();
    if (cursor_y < ansi_scroll_top || cursor_y > ansi_scroll_bottom) {
        return;
    }

    int top = cursor_y;
    int bottom = ansi_scroll_bottom;
    int region_rows = bottom - top + 1;
    if (region_rows <= 0) {
        return;
    }
    if (n > region_rows) {
        n = region_rows;
    }

    bool render_now = !(backend == SCREEN_BACKEND_FRAMEBUFFER && scrollback_view_offset > 0);

    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
            fb_render_cell(cursor_drawn_x, cursor_drawn_y);
            cursor_drawn_x = -1;
            cursor_drawn_y = -1;
        }
        if (mouse_cursor_enabled && mouse_drawn_x >= 0 && mouse_drawn_y >= 0) {
            fb_render_cell(mouse_drawn_x, mouse_drawn_y);
        }

        int cols = screen_cols_value;
        size_t row_bytes = (size_t)cols * sizeof(fb_cell_t);
        int move_rows = region_rows - n;
        if (move_rows > 0) {
            memmove(&fb_cells[top * cols], &fb_cells[(top + n) * cols], row_bytes * (size_t)move_rows);
        }

        fb_cell_t blank = fb_cell_make((uint8_t)' ', current_fg, current_bg);
        for (int y = bottom - n + 1; y <= bottom; y++) {
            fb_cell_t* row = &fb_cells[y * cols];
            for (int x = 0; x < cols; x++) {
                row[x] = blank;
            }
        }

        if (render_now) {
            uint32_t region_y = fb_origin_y + (uint32_t)top * fb_font.height;
            uint32_t shift_px = (uint32_t)n * fb_font.height;
            uint32_t region_px_height = (uint32_t)region_rows * fb_font.height;
            if (shift_px >= region_px_height) {
                shift_px = region_px_height;
            }
            uint32_t copy_bytes = (region_px_height - shift_px) * fb_pitch;

            uint8_t* dst = fb_addr + region_y * fb_pitch;
            uint8_t* src = dst + shift_px * fb_pitch;
            memmove(dst, src, copy_bytes);

            uint32_t bg_px = fb_color_from_xterm(current_bg);
            uint32_t clear_y = region_y + (region_px_height - shift_px);
            fb_fill_rect(0, clear_y, fb_width, shift_px, bg_px);
        }

        if (mouse_cursor_enabled) {
            fb_update_mouse_cursor();
        }
    } else {
        // Shift physical rows up by N within the scroll region.
        for (int y = top; y <= bottom - n; y++) {
            int dst_y = screen_phys_y(y);
            int src_y = screen_phys_y(y + n);
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_BUFFER[dst_y * VGA_WIDTH + x] = VGA_BUFFER[src_y * VGA_WIDTH + x];
            }
        }

        for (int y = bottom - n + 1; y <= bottom; y++) {
            int phys_y = screen_phys_y(y);
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_BUFFER[phys_y * VGA_WIDTH + x] = vga_entry(' ', current_color);
            }
        }
    }
}

static void ansi_delete_chars(int n) {
    if (n < 1) {
        n = 1;
    }

    int y = cursor_y;
    if (y < 0 || y >= usable_height()) {
        return;
    }

    if (cursor_x < 0) {
        cursor_x = 0;
    }
    if (cursor_x >= screen_cols_value) {
        return;
    }

    int cols = screen_cols_value;
    if (n > cols - cursor_x) {
        n = cols - cursor_x;
    }
    if (n <= 0) {
        return;
    }

    bool render_now = !(backend == SCREEN_BACKEND_FRAMEBUFFER && scrollback_view_offset > 0);

    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
            fb_render_cell(cursor_drawn_x, cursor_drawn_y);
            cursor_drawn_x = -1;
            cursor_drawn_y = -1;
        }

        fb_cell_t* row = &fb_cells[y * cols];
        size_t move_bytes = (size_t)(cols - cursor_x - n) * sizeof(fb_cell_t);
        if (move_bytes > 0) {
            memmove(&row[cursor_x], &row[cursor_x + n], move_bytes);
        }

        fb_cell_t blank = fb_cell_make((uint8_t)' ', current_fg, current_bg);
        for (int x = cols - n; x < cols; x++) {
            row[x] = blank;
        }

        if (render_now) {
            for (int x = cursor_x; x < cols; x++) {
                fb_render_cell(x, y);
            }
        }
    } else {
        int phys_y = screen_phys_y(y);
        for (int x = cursor_x; x < cols - n; x++) {
            VGA_BUFFER[phys_y * VGA_WIDTH + screen_phys_x(x)] =
                VGA_BUFFER[phys_y * VGA_WIDTH + screen_phys_x(x + n)];
        }
        for (int x = cols - n; x < cols; x++) {
            VGA_BUFFER[phys_y * VGA_WIDTH + screen_phys_x(x)] = vga_entry(' ', current_color);
        }
    }
}

static void ansi_reset(void) {
    ansi_state = ANSI_STATE_NONE;
    ansi_param_count = 0;
    ansi_current = -1;
    ansi_private = false;
    for (uint32_t i = 0; i < (uint32_t)(sizeof(ansi_params) / sizeof(ansi_params[0])); i++) {
        ansi_params[i] = 0;
    }
}

static void ansi_push_param(void) {
    if (ansi_param_count >= (int)(sizeof(ansi_params) / sizeof(ansi_params[0]))) {
        ansi_current = -1;
        return;
    }
    ansi_params[ansi_param_count++] = (ansi_current < 0) ? 0 : ansi_current;
    ansi_current = -1;
}

static int ansi_get_param(int idx, int def) {
    if (idx < 0) {
        return def;
    }
    if (idx < ansi_param_count) {
        int v = ansi_params[idx];
        return (v == 0) ? def : v;
    }
    if (idx == ansi_param_count && ansi_current >= 0) {
        int v = ansi_current;
        return (v == 0) ? def : v;
    }
    return def;
}

static uint8_t ansi_basic_to_vga(uint8_t idx) {
    static const uint8_t map[8] = {
        VGA_BLACK,      // 30 black
        VGA_RED,        // 31 red
        VGA_GREEN,      // 32 green
        VGA_BROWN,      // 33 yellow (dim)
        VGA_BLUE,       // 34 blue
        VGA_MAGENTA,    // 35 magenta
        VGA_CYAN,       // 36 cyan
        VGA_LIGHT_GREY, // 37 white (dim)
    };
    if (idx < 8u) {
        return map[idx];
    }
    return VGA_LIGHT_GREY;
}

static uint32_t ansi_clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint32_t)v;
}

static void ansi_apply_sgr_params(void) {
    if (ansi_param_count == 0) {
        current_fg = default_fg;
        current_bg = default_bg;
        update_vga_colors();
        return;
    }

    for (int i = 0; i < ansi_param_count; i++) {
        int p = ansi_params[i];

        if (p == 0) {
            current_fg = default_fg;
            current_bg = default_bg;
            continue;
        }
        if (p == 1) { // bold/bright (only affects the base 16 colors)
            if (current_fg < 8u) {
                current_fg = (uint8_t)(current_fg + 8u);
            }
            continue;
        }
        if (p == 22) { // normal intensity
            if (current_fg >= 8u && current_fg <= 15u) {
                current_fg = (uint8_t)(current_fg - 8u);
            }
            continue;
        }
        if (p == 7) { // reverse video
            uint8_t tmp = current_fg;
            current_fg = current_bg;
            current_bg = tmp;
            continue;
        }
        if (p == 27) { // reverse off (best-effort)
            current_fg = default_fg;
            current_bg = default_bg;
            continue;
        }

        if (p >= 30 && p <= 37) {
            current_fg = ansi_basic_to_vga((uint8_t)(p - 30));
            continue;
        }
        if (p >= 90 && p <= 97) {
            current_fg = (uint8_t)(ansi_basic_to_vga((uint8_t)(p - 90)) + 8u);
            continue;
        }
        if (p >= 40 && p <= 47) {
            current_bg = ansi_basic_to_vga((uint8_t)(p - 40));
            continue;
        }
        if (p >= 100 && p <= 107) {
            current_bg = (uint8_t)(ansi_basic_to_vga((uint8_t)(p - 100)) + 8u);
            continue;
        }
        if (p == 39) { // default fg
            current_fg = default_fg;
            continue;
        }
        if (p == 49) { // default bg
            current_bg = default_bg;
            continue;
        }

        // 256-color / truecolor sequences: 38;5;N / 48;5;N or 38;2;R;G;B / 48;2;R;G;B.
        if ((p == 38 || p == 48) && (i + 1) < ansi_param_count) {
            int mode = ansi_params[i + 1];
            if (mode == 5 && (i + 2) < ansi_param_count) {
                uint32_t idx = ansi_clamp_u8(ansi_params[i + 2]);
                if (p == 38) current_fg = (uint8_t)idx;
                else current_bg = (uint8_t)idx;
                i += 2;
                continue;
            }
            if (mode == 2 && (i + 4) < ansi_param_count) {
                uint8_t r = (uint8_t)ansi_clamp_u8(ansi_params[i + 2]);
                uint8_t g = (uint8_t)ansi_clamp_u8(ansi_params[i + 3]);
                uint8_t b = (uint8_t)ansi_clamp_u8(ansi_params[i + 4]);
                uint8_t idx = xterm_index_from_rgb(r, g, b);
                if (p == 38) current_fg = idx;
                else current_bg = idx;
                i += 4;
                continue;
            }
        }
    }

    update_vga_colors();
}

static bool ansi_handle_char(char c) {
    if (ansi_state == ANSI_STATE_NONE) {
        if ((uint8_t)c == 0x1Bu) {
            ansi_state = ANSI_STATE_ESC;
            return true;
        }
        return false;
    }

    if (ansi_state == ANSI_STATE_ESC) {
        if (c == '[') {
            ansi_state = ANSI_STATE_CSI;
            ansi_param_count = 0;
            ansi_current = -1;
            ansi_private = false;
            for (uint32_t i = 0; i < (uint32_t)(sizeof(ansi_params) / sizeof(ansi_params[0])); i++) {
                ansi_params[i] = 0;
            }
            return true;
        }
        if (c == '7') { // DECSC save cursor
            ansi_saved_x = cursor_x;
            ansi_saved_y = cursor_y;
            ansi_reset();
            return true;
        }
        if (c == '8') { // DECRC restore cursor
            cursor_x = ansi_saved_x;
            cursor_y = ansi_saved_y;
            cursor_clamp();
            update_cursor();
            ansi_reset();
            return true;
        }

        ansi_reset();
        return true;
    }

    // CSI state.
    if (c == '?' && ansi_param_count == 0 && ansi_current < 0 && !ansi_private) {
        ansi_private = true;
        return true;
    }
    if (c >= '0' && c <= '9') {
        if (ansi_current < 0) {
            ansi_current = 0;
        }
        ansi_current = ansi_current * 10 + (c - '0');
        return true;
    }
    if (c == ';') {
        ansi_push_param();
        return true;
    }

    // Final byte.
    if (ansi_current >= 0) {
        ansi_push_param();
    }

    switch (c) {
        case 'A': { // cursor up
            int n = ansi_get_param(0, 1);
            cursor_y -= n;
            cursor_clamp();
            update_cursor();
            break;
        }
        case 'B': { // cursor down
            int n = ansi_get_param(0, 1);
            cursor_y += n;
            cursor_clamp();
            update_cursor();
            break;
        }
        case 'C': { // cursor forward
            int n = ansi_get_param(0, 1);
            cursor_x += n;
            cursor_clamp();
            update_cursor();
            break;
        }
        case 'D': { // cursor back
            int n = ansi_get_param(0, 1);
            cursor_x -= n;
            cursor_clamp();
            update_cursor();
            break;
        }
        case 'H': // cursor position
        case 'f': {
            int row = ansi_get_param(0, 1);
            int col = ansi_get_param(1, 1);
            cursor_y = row - 1;
            cursor_x = col - 1;
            cursor_clamp();
            update_cursor();
            break;
        }
        case 'G': { // cursor horizontal absolute
            int col = ansi_get_param(0, 1);
            cursor_x = col - 1;
            cursor_clamp();
            update_cursor();
            break;
        }
        case 'K': { // erase in line (0 = to end)
            int mode = ansi_get_param(0, 0);
            if (mode == 0) {
                ansi_erase_to_eol();
            } else {
                ansi_erase_line(mode);
            }
            update_cursor();
            break;
        }
        case 'J': { // erase in display (2 = clear)
            int mode = ansi_get_param(0, 0);
            ansi_erase_display(mode);
            update_cursor();
            break;
        }
        case 'r': { // set scrolling region
            int height = usable_height();
            if (height < 1) height = 1;

            int top = ansi_get_param(0, 1);
            int bottom = ansi_get_param(1, height);
            if (top <= 0) top = 1;
            if (bottom <= 0) bottom = height;

            ansi_scroll_top = top - 1;
            ansi_scroll_bottom = bottom - 1;
            ansi_scroll_region_clamp();

            cursor_x = 0;
            cursor_y = 0;
            cursor_clamp();
            update_cursor();
            break;
        }
        case 'S': { // scroll up
            int n = ansi_get_param(0, 1);
            if (n < 1) n = 1;
            int saved_x = cursor_x;
            int saved_y = cursor_y;
            for (int i = 0; i < n; i++) {
                if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
                    fb_scroll();
                } else {
                    vga_scroll();
                }
            }
            cursor_x = saved_x;
            cursor_y = saved_y;
            cursor_clamp();
            update_cursor();
            break;
        }
        case 'T': { // scroll down
            int n = ansi_get_param(0, 1);
            if (n < 1) n = 1;
            for (int i = 0; i < n; i++) {
                if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
                    fb_scroll_down();
                } else {
                    vga_scroll_down();
                }
            }
            cursor_clamp();
            update_cursor();
            break;
        }
        case 's': { // save cursor
            ansi_saved_x = cursor_x;
            ansi_saved_y = cursor_y;
            break;
        }
        case 'u': { // restore cursor
            cursor_x = ansi_saved_x;
            cursor_y = ansi_saved_y;
            cursor_clamp();
            update_cursor();
            break;
        }
        case 'm': { // SGR (colors)
            ansi_apply_sgr_params();
            break;
        }
        case 'L': { // insert line(s)
            int n = ansi_get_param(0, 1);
            ansi_insert_lines(n);
            update_cursor();
            break;
        }
        case 'M': { // delete line(s)
            int n = ansi_get_param(0, 1);
            ansi_delete_lines(n);
            update_cursor();
            break;
        }
        case 'P': { // delete character(s)
            int n = ansi_get_param(0, 1);
            ansi_delete_chars(n);
            update_cursor();
            break;
        }
        case 'h':
        case 'l': {
            if (ansi_private) {
                bool set = (c == 'h');
                for (int i = 0; i < ansi_param_count; i++) {
                    int p = ansi_params[i];
                    if (p == 25) {
                        cursor_vt_hidden = !set;
                        update_cursor();
                        continue;
                    }
                    // Xterm/VT100 mouse reporting (used by curses/TUIs).
                    if (p == 1000) {
                        if (set) vt_mouse_mode_mask |= VT_MOUSE_MODE_1000;
                        else vt_mouse_mode_mask &= (uint8_t)~VT_MOUSE_MODE_1000;
                        continue;
                    }
                    if (p == 1002) {
                        if (set) vt_mouse_mode_mask |= VT_MOUSE_MODE_1002;
                        else vt_mouse_mode_mask &= (uint8_t)~VT_MOUSE_MODE_1002;
                        continue;
                    }
                    if (p == 1003) {
                        if (set) vt_mouse_mode_mask |= VT_MOUSE_MODE_1003;
                        else vt_mouse_mode_mask &= (uint8_t)~VT_MOUSE_MODE_1003;
                        continue;
                    }
                    // SGR mouse mode: CSI <b;x;yM / CSI <b;x;ym
                    if (p == 1006) {
                        vt_mouse_sgr = set;
                        continue;
                    }
                }
            }
            break;
        }
        default:
            break;
    }

    ansi_reset();
    return true;
}

int screen_cols(void) {
    return screen_cols_value;
}

int screen_rows(void) {
    return screen_rows_value;
}

int screen_usable_rows(void) {
    return usable_height();
}

static inline int screen_phys_x(int x) {
    return x + pad_left_cols;
}

static inline int screen_phys_y(int y) {
    return y + pad_top_rows;
}

static void vga_hw_cursor_update(void) {
    int phys_x = screen_phys_x(cursor_x);
    int phys_y = screen_phys_y(cursor_y);
    if (phys_x < 0) phys_x = 0;
    if (phys_y < 0) phys_y = 0;
    if (phys_x >= VGA_WIDTH) phys_x = VGA_WIDTH - 1;
    if (phys_y >= VGA_HEIGHT) phys_y = VGA_HEIGHT - 1;
    uint16_t pos = (uint16_t)(phys_y * VGA_WIDTH + phys_x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void vga_hw_cursor_set_enabled(bool enabled) {
    outb(0x3D4, 0x0A);
    uint8_t cur_start = inb(0x3D5);
    if (enabled) {
        cur_start &= (uint8_t)~0x20;
    } else {
        cur_start |= 0x20;
    }
    outb(0x3D4, 0x0A);
    outb(0x3D5, cur_start);
}

static uint32_t fb_pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t value = 0;

    if (fb_r_size) {
        uint32_t mask = (fb_r_size >= 32) ? 0xFFFFFFFFu : ((1u << fb_r_size) - 1u);
        uint32_t component = (uint32_t)r;
        if (fb_r_size < 8) component >>= (8u - fb_r_size);
        value |= (component & mask) << fb_r_pos;
    }
    if (fb_g_size) {
        uint32_t mask = (fb_g_size >= 32) ? 0xFFFFFFFFu : ((1u << fb_g_size) - 1u);
        uint32_t component = (uint32_t)g;
        if (fb_g_size < 8) component >>= (8u - fb_g_size);
        value |= (component & mask) << fb_g_pos;
    }
    if (fb_b_size) {
        uint32_t mask = (fb_b_size >= 32) ? 0xFFFFFFFFu : ((1u << fb_b_size) - 1u);
        uint32_t component = (uint32_t)b;
        if (fb_b_size < 8) component >>= (8u - fb_b_size);
        value |= (component & mask) << fb_b_pos;
    }

    return value;
}

static uint32_t fb_color_from_vga(uint8_t idx) {
    idx &= 0x0F;
    uint8_t r = vga_palette_rgb[idx][0];
    uint8_t g = vga_palette_rgb[idx][1];
    uint8_t b = vga_palette_rgb[idx][2];
    return fb_pack_rgb(r, g, b);
}

static uint32_t fb_xterm_palette_px[256];
static bool fb_xterm_palette_ready = false;

static void fb_build_xterm_palette(void) {
    if (fb_xterm_palette_ready) {
        return;
    }
    if (fb_bytes_per_pixel == 0 || fb_addr == 0 || fb_pitch == 0 || fb_width == 0 || fb_height == 0) {
        for (uint32_t i = 0; i < 256u; i++) {
            fb_xterm_palette_px[i] = 0;
        }
        fb_xterm_palette_ready = true;
        return;
    }

    for (uint32_t i = 0; i < 256u; i++) {
        uint8_t r = 0, g = 0, b = 0;
        xterm_rgb_from_index((uint8_t)i, &r, &g, &b);
        fb_xterm_palette_px[i] = fb_pack_rgb(r, g, b);
    }

    fb_xterm_palette_ready = true;
}

static uint32_t fb_color_from_xterm(uint8_t idx) {
    if (!fb_xterm_palette_ready) {
        fb_build_xterm_palette();
    }
    return fb_xterm_palette_px[idx];
}

static void fb_put_pixel(uint32_t x, uint32_t y, uint32_t pixel) {
    uint8_t* p = fb_addr + y * fb_pitch + x * (uint32_t)fb_bytes_per_pixel;
    switch (fb_bytes_per_pixel) {
        case 4:
            *(uint32_t*)p = pixel;
            break;
        case 3:
            p[0] = (uint8_t)(pixel & 0xFFu);
            p[1] = (uint8_t)((pixel >> 8) & 0xFFu);
            p[2] = (uint8_t)((pixel >> 16) & 0xFFu);
            break;
        case 2:
            *(uint16_t*)p = (uint16_t)(pixel & 0xFFFFu);
            break;
        default:
            *p = (uint8_t)(pixel & 0xFFu);
            break;
    }
}

static void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t pixel) {
    for (uint32_t yy = 0; yy < h; yy++) {
        uint8_t* row = fb_addr + (y + yy) * fb_pitch;
        for (uint32_t xx = 0; xx < w; xx++) {
            uint8_t* p = row + (x + xx) * (uint32_t)fb_bytes_per_pixel;
            switch (fb_bytes_per_pixel) {
                case 4:
                    *(uint32_t*)p = pixel;
                    break;
                case 3:
                    p[0] = (uint8_t)(pixel & 0xFFu);
                    p[1] = (uint8_t)((pixel >> 8) & 0xFFu);
                    p[2] = (uint8_t)((pixel >> 16) & 0xFFu);
                    break;
                case 2:
                    *(uint16_t*)p = (uint16_t)(pixel & 0xFFFFu);
                    break;
                default:
                    *p = (uint8_t)(pixel & 0xFFu);
                    break;
            }
        }
    }
}

static void fb_render_entry(int x, int y, fb_cell_t entry) {
    if (x < 0 || y < 0 || x >= screen_cols_value || y >= screen_rows_value) {
        return;
    }

    uint8_t ch = fb_cell_ch(entry);
    uint8_t fg = fb_cell_fg(entry);
    uint8_t bg = fb_cell_bg(entry);

    uint32_t fg_px = fb_color_from_xterm(fg);
    uint32_t bg_px = fb_color_from_xterm(bg);

    uint32_t base_x = fb_origin_x + (uint32_t)x * fb_font.width;
    uint32_t base_y = fb_origin_y + (uint32_t)y * fb_font.height;

    uint32_t glyph_idx = (uint32_t)ch;
    if (glyph_idx >= fb_font.glyph_count) {
        glyph_idx = (uint32_t)'?';
        if (glyph_idx >= fb_font.glyph_count) {
            glyph_idx = 0;
        }
    }
    const uint8_t* glyph = fb_font.glyphs + glyph_idx * fb_font.bytes_per_glyph;

    for (uint32_t row = 0; row < fb_font.height; row++) {
        const uint8_t* row_data = glyph + row * fb_font.row_bytes;
        for (uint32_t col = 0; col < fb_font.width; col++) {
            uint32_t px = base_x + col;
            uint32_t py = base_y + row;
            uint8_t byte = row_data[col / 8u];
            bool on = (byte & (uint8_t)(0x80u >> (col & 7u))) != 0;
            fb_put_pixel(px, py, on ? fg_px : bg_px);
        }
    }
}

static void fb_render_cell(int x, int y) {
    if (x < 0 || y < 0 || x >= screen_cols_value || y >= screen_rows_value) {
        return;
    }
    fb_render_entry(x, y, fb_cells[y * screen_cols_value + x]);
}

static uint32_t fb_cursor_thickness(void) {
    return (fb_font.height >= 16u) ? 2u : 1u;
}

static void fb_draw_cursor_overlay(int x, int y) {
    if (x < 0 || y < 0 || x >= screen_cols_value || y >= usable_height()) {
        return;
    }

    fb_cell_t entry = fb_cells[y * screen_cols_value + x];
    uint8_t fg = fb_cell_fg(entry);
    uint32_t fg_px = fb_color_from_xterm(fg);

    uint32_t base_x = fb_origin_x + (uint32_t)x * fb_font.width;
    uint32_t base_y = fb_origin_y + (uint32_t)y * fb_font.height;
    uint32_t thickness = fb_cursor_thickness();
    uint32_t y0 = base_y + (fb_font.height - thickness);
    fb_fill_rect(base_x, y0, fb_font.width, thickness, fg_px);
}

static void fb_draw_mouse_cursor_overlay(int x, int y) {
    if (x < 0 || y < 0 || x >= screen_cols_value || y >= usable_height()) {
        return;
    }

    uint32_t base_x = fb_origin_x + (uint32_t)x * fb_font.width;
    uint32_t base_y = fb_origin_y + (uint32_t)y * fb_font.height;
    uint32_t w = fb_font.width;
    uint32_t h = fb_font.height;
    uint32_t px = fb_color_from_vga(VGA_LIGHT_CYAN);

    // Small filled arrow in the top-left of the cell.
    uint32_t aw = (w < 9u) ? w : 9u;
    uint32_t ah = (h < 13u) ? h : 13u;
    if (aw < 2u || ah < 2u) {
        fb_put_pixel(base_x, base_y, px);
        return;
    }

    for (uint32_t yy = 0; yy < ah; yy++) {
        uint32_t roww = yy + 1u;
        if (roww > aw) roww = aw;
        for (uint32_t xx = 0; xx < roww; xx++) {
            fb_put_pixel(base_x + xx, base_y + yy, px);
        }
    }

    uint32_t stem_y = ah;
    uint32_t stem_h = 4u;
    if (stem_y + stem_h > h) stem_h = (stem_y < h) ? (h - stem_y) : 0u;
    for (uint32_t yy = 0; yy < stem_h; yy++) {
        fb_put_pixel(base_x + 0u, base_y + stem_y + yy, px);
        if (aw > 2u) {
            fb_put_pixel(base_x + 1u, base_y + stem_y + yy, px);
        }
    }
}

static void fb_update_mouse_cursor(void) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        mouse_drawn_x = -1;
        mouse_drawn_y = -1;
        return;
    }

    if (!mouse_cursor_enabled) {
        if (mouse_drawn_x >= 0 && mouse_drawn_y >= 0) {
            int old_x = mouse_drawn_x;
            int old_y = mouse_drawn_y;
            fb_render_cell(old_x, old_y);
            mouse_drawn_x = -1;
            mouse_drawn_y = -1;
            if (!cursor_force_hidden && !cursor_vt_hidden && cursor_enabled && old_x == cursor_x && old_y == cursor_y) {
                fb_draw_cursor_overlay(cursor_x, cursor_y);
            }
        }
        return;
    }

    int height = usable_height();
    if (height < 1) height = 1;
    if (mouse_cursor_x < 0) mouse_cursor_x = 0;
    if (mouse_cursor_y < 0) mouse_cursor_y = 0;
    if (mouse_cursor_x >= screen_cols_value) mouse_cursor_x = screen_cols_value - 1;
    if (mouse_cursor_y >= height) mouse_cursor_y = height - 1;

    if (mouse_drawn_x != mouse_cursor_x || mouse_drawn_y != mouse_cursor_y) {
        if (mouse_drawn_x >= 0 && mouse_drawn_y >= 0) {
            int old_x = mouse_drawn_x;
            int old_y = mouse_drawn_y;
            fb_render_cell(old_x, old_y);
            if (!cursor_force_hidden && !cursor_vt_hidden && cursor_enabled && old_x == cursor_x && old_y == cursor_y) {
                fb_draw_cursor_overlay(cursor_x, cursor_y);
            }
        }
        mouse_drawn_x = mouse_cursor_x;
        mouse_drawn_y = mouse_cursor_y;
    }

    fb_draw_mouse_cursor_overlay(mouse_drawn_x, mouse_drawn_y);
}

static void fb_update_cursor(void) {
    int old_x = cursor_drawn_x;
    int old_y = cursor_drawn_y;

    if (cursor_force_hidden || cursor_vt_hidden || !cursor_enabled) {
        if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
            fb_render_cell(cursor_drawn_x, cursor_drawn_y);
            cursor_drawn_x = -1;
            cursor_drawn_y = -1;
            if (mouse_cursor_enabled && mouse_drawn_x == old_x && mouse_drawn_y == old_y) {
                fb_draw_mouse_cursor_overlay(mouse_drawn_x, mouse_drawn_y);
            }
        }
        return;
    }

    if (cursor_drawn_x != cursor_x || cursor_drawn_y != cursor_y) {
        if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
            fb_render_cell(cursor_drawn_x, cursor_drawn_y);
        }
        cursor_drawn_x = cursor_x;
        cursor_drawn_y = cursor_y;
    }

    fb_draw_cursor_overlay(cursor_x, cursor_y);
}

static void update_cursor(void) {
    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        fb_update_cursor();
    } else {
        if (cursor_force_hidden || cursor_vt_hidden || !cursor_enabled) {
            vga_hw_cursor_set_enabled(false);
            return;
        }
        vga_hw_cursor_set_enabled(true);
        vga_hw_cursor_update();
    }
}

static void vga_scroll(void) {
    ansi_scroll_region_clamp();
    int top = ansi_scroll_top;
    int bottom = ansi_scroll_bottom;
    int height = usable_height();
    if (height < 1) height = 1;

    if (top < 0) top = 0;
    if (bottom < 0) bottom = 0;
    if (top >= height) top = 0;
    if (bottom >= height) bottom = height - 1;

    if (bottom <= top) {
        cursor_y = top;
        return;
    }

    // Save the line that is about to scroll out (top of the scroll region).
    int cols = screen_cols_value;
    if (cols > FB_MAX_COLS) cols = FB_MAX_COLS;
    if (cols > 0) {
        fb_cell_t line[FB_MAX_COLS];
        for (int x = 0; x < cols; x++) {
            uint16_t v = VGA_BUFFER[screen_phys_y(top) * VGA_WIDTH + screen_phys_x(x)];
            uint8_t ch = (uint8_t)(v & 0xFFu);
            uint8_t attr = (uint8_t)((v >> 8) & 0xFFu);
            uint8_t fg = (uint8_t)(attr & 0x0Fu);
            uint8_t bg = (uint8_t)((attr >> 4) & 0x0Fu);
            line[x] = fb_cell_make(ch, fg, bg);
        }
        scrollback_push_line(line, cols);
    }

    // Move all lines in the scroll region up by one.
    for (int y = top; y < bottom; y++) {
        int dst_y = screen_phys_y(y);
        int src_y = screen_phys_y(y + 1);
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[dst_y * VGA_WIDTH + x] = VGA_BUFFER[src_y * VGA_WIDTH + x];
        }
    }

    // Clear the last line in the scroll region.
    int last_y = screen_phys_y(bottom);
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[last_y * VGA_WIDTH + x] = vga_entry(' ', current_color);
    }

    cursor_y = bottom;
}

static void fb_scroll(void) {
    ansi_scroll_region_clamp();
    int top = ansi_scroll_top;
    int bottom = ansi_scroll_bottom;
    int height = usable_height();
    if (height < 1) height = 1;

    if (top < 0) top = 0;
    if (bottom < 0) bottom = 0;
    if (top >= height) top = 0;
    if (bottom >= height) bottom = height - 1;

    if (bottom <= top) {
        cursor_y = top;
        return;
    }

    // The framebuffer cursor is an overlay drawn directly into pixel memory.
    // If we scroll by memcpy-ing framebuffer rows, that overlay will get copied too,
    // leaving "underscore trails" behind. Undraw it before copying any pixels.
    if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
        fb_render_cell(cursor_drawn_x, cursor_drawn_y);
        cursor_drawn_x = -1;
        cursor_drawn_y = -1;
    }
    if (mouse_cursor_enabled && mouse_drawn_x >= 0 && mouse_drawn_y >= 0) {
        fb_render_cell(mouse_drawn_x, mouse_drawn_y);
    }

    int cols = screen_cols_value;
    if (cols > 0) {
        scrollback_push_line(&fb_cells[top * cols], cols);
    }
    size_t row_bytes = (size_t)cols * sizeof(fb_cell_t);
    memmove(&fb_cells[top * cols], &fb_cells[(top + 1) * cols], row_bytes * (size_t)(bottom - top));

    fb_cell_t blank = fb_cell_make((uint8_t)' ', current_fg, current_bg);
    for (int x = 0; x < cols; x++) {
        fb_cells[bottom * cols + x] = blank;
    }

    if (scrollback_view_offset == 0) {
        uint32_t region_rows = (uint32_t)(bottom - top + 1);
        uint32_t region_px_height = region_rows * fb_font.height;
        uint32_t copy_bytes = (region_px_height - fb_font.height) * fb_pitch;
        uint32_t region_y = fb_origin_y + (uint32_t)top * fb_font.height;

        uint8_t* dst = fb_addr + region_y * fb_pitch;
        uint8_t* src = dst + fb_font.height * fb_pitch;
        memmove(dst, src, copy_bytes);

        uint32_t bg_px = fb_color_from_xterm(current_bg);
        uint32_t clear_y = fb_origin_y + (uint32_t)bottom * fb_font.height;
        fb_fill_rect(0, clear_y, fb_width, fb_font.height, bg_px);
    }

    cursor_y = bottom;
    if (mouse_cursor_enabled) {
        fb_update_mouse_cursor();
    }
}

static void vga_scroll_down(void) {
    ansi_scroll_region_clamp();
    int top = ansi_scroll_top;
    int bottom = ansi_scroll_bottom;
    int height = usable_height();
    if (height < 1) height = 1;

    if (top < 0) top = 0;
    if (bottom < 0) bottom = 0;
    if (top >= height) top = 0;
    if (bottom >= height) bottom = height - 1;

    if (bottom <= top) {
        return;
    }

    // Move all lines in the scroll region down by one.
    for (int y = bottom; y > top; y--) {
        int dst_y = screen_phys_y(y);
        int src_y = screen_phys_y(y - 1);
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[dst_y * VGA_WIDTH + x] = VGA_BUFFER[src_y * VGA_WIDTH + x];
        }
    }

    // Clear the first line in the scroll region.
    int first_y = screen_phys_y(top);
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[first_y * VGA_WIDTH + x] = vga_entry(' ', current_color);
    }
}

static void fb_scroll_down(void) {
    ansi_scroll_region_clamp();
    int top = ansi_scroll_top;
    int bottom = ansi_scroll_bottom;
    int height = usable_height();
    if (height < 1) height = 1;

    if (top < 0) top = 0;
    if (bottom < 0) bottom = 0;
    if (top >= height) top = 0;
    if (bottom >= height) bottom = height - 1;

    if (bottom <= top) {
        return;
    }

    if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
        fb_render_cell(cursor_drawn_x, cursor_drawn_y);
        cursor_drawn_x = -1;
        cursor_drawn_y = -1;
    }
    if (mouse_cursor_enabled && mouse_drawn_x >= 0 && mouse_drawn_y >= 0) {
        fb_render_cell(mouse_drawn_x, mouse_drawn_y);
    }

    int cols = screen_cols_value;
    size_t row_bytes = (size_t)cols * sizeof(fb_cell_t);
    memmove(&fb_cells[(top + 1) * cols], &fb_cells[top * cols], row_bytes * (size_t)(bottom - top));

    fb_cell_t blank = fb_cell_make((uint8_t)' ', current_fg, current_bg);
    for (int x = 0; x < cols; x++) {
        fb_cells[top * cols + x] = blank;
    }

    if (scrollback_view_offset == 0) {
        uint32_t region_rows = (uint32_t)(bottom - top + 1);
        uint32_t region_px_height = region_rows * fb_font.height;
        uint32_t copy_bytes = (region_px_height - fb_font.height) * fb_pitch;
        uint32_t region_y = fb_origin_y + (uint32_t)top * fb_font.height;

        uint8_t* src = fb_addr + region_y * fb_pitch;
        uint8_t* dst = src + fb_font.height * fb_pitch;
        memmove(dst, src, copy_bytes);

        uint32_t bg_px = fb_color_from_xterm(current_bg);
        fb_fill_rect(0, region_y, fb_width, fb_font.height, bg_px);
    }

    if (mouse_cursor_enabled) {
        fb_update_mouse_cursor();
    }
}

static void scrollback_render_view(void) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return;
    }

    int rows = usable_height();
    int cols = screen_cols_value;
    if (rows < 1 || cols < 1) {
        return;
    }
    if (cols > FB_MAX_COLS) {
        cols = FB_MAX_COLS;
    }

    uint32_t history = scrollback_count;
    uint32_t offset = scrollback_view_offset;
    if (offset > history) {
        offset = history;
    }
    uint32_t start = history - offset;

    for (int y = 0; y < rows; y++) {
        uint32_t doc_idx = start + (uint32_t)y;
        const fb_cell_t* src = 0;
        bool is_history = doc_idx < history;
        if (is_history) {
            src = scrollback_line_ptr(doc_idx);
        } else {
            uint32_t live_row = doc_idx - history;
            if (live_row >= (uint32_t)FB_MAX_ROWS) {
                continue;
            }
            src = &fb_cells[live_row * (uint32_t)screen_cols_value];
        }

        for (int x = 0; x < cols; x++) {
            fb_render_entry(x, y, src[x]);
        }
    }
}

static void scrollback_render_bottom(void) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return;
    }

    int rows = usable_height();
    int cols = screen_cols_value;
    if (rows < 1 || cols < 1) {
        return;
    }

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            fb_render_cell(x, y);
        }
    }
}

bool screen_scrollback_active(void) {
    return scrollback_view_offset > 0;
}

void screen_scrollback_lines(int32_t delta) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return;
    }
    if (scrollback_count == 0) {
        return;
    }

    int32_t new_offset = (int32_t)scrollback_view_offset + delta;
    if (new_offset < 0) {
        new_offset = 0;
    }
    if ((uint32_t)new_offset > scrollback_count) {
        new_offset = (int32_t)scrollback_count;
    }
    if ((uint32_t)new_offset == scrollback_view_offset) {
        return;
    }

    scrollback_view_offset = (uint32_t)new_offset;
    cursor_force_hidden = scrollback_view_offset > 0;
    update_cursor();

    if (scrollback_view_offset > 0) {
        scrollback_render_view();
    } else {
        scrollback_render_bottom();
        update_cursor();
    }
}

void screen_scrollback_reset(void) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return;
    }
    if (scrollback_view_offset == 0) {
        return;
    }
    scrollback_view_offset = 0;
    cursor_force_hidden = false;
    scrollback_render_bottom();
    update_cursor();
}

void screen_clear(void) {
    scrollback_view_offset = 0;
    cursor_force_hidden = false;

    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        fb_cell_t blank = fb_cell_make((uint8_t)' ', current_fg, current_bg);
        int cols = screen_cols_value;
        int rows = screen_rows_value;

        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                fb_cells[y * cols + x] = blank;
            }
        }

        uint32_t bg_px = fb_color_from_xterm(current_bg);
        fb_fill_rect(0, 0, fb_width, fb_height, bg_px);
    } else {
        for (int y = 0; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', current_color);
            }
        }
    }

    cursor_x = 0;
    cursor_y = 0;
    cursor_drawn_x = -1;
    cursor_drawn_y = -1;
    update_cursor();
    fb_update_mouse_cursor();
}

void screen_init(uint32_t multiboot_magic, uint32_t* mboot_info) {
    default_fg = VGA_WHITE;
    default_bg = VGA_BLUE;
    current_fg = default_fg;
    current_bg = default_bg;
    update_vga_colors();
    reserved_bottom_rows = 0;
    cursor_x = 0;
    cursor_y = 0;
    cursor_enabled = true;
    cursor_drawn_x = -1;
    cursor_drawn_y = -1;
    mouse_cursor_enabled = false;
    mouse_cursor_x = 0;
    mouse_cursor_y = 0;
    mouse_drawn_x = -1;
    mouse_drawn_y = -1;
    vt_mouse_mode_mask = 0;
    vt_mouse_sgr = false;
    utf8_state = (utf8_state_t){0};
    fb_unicode_map_count = 0;
    fb_unicode_replacement_glyph = (uint8_t)'?';
    fb_unicode_ready = false;
    fb_xterm_palette_ready = false;

    backend = SCREEN_BACKEND_VGA_TEXT;
    pad_left_cols = 1;
    pad_right_cols = 1;
    pad_top_rows = 1;
    pad_bottom_rows = 1;
    if (VGA_WIDTH <= (pad_left_cols + pad_right_cols)) {
        pad_left_cols = 0;
        pad_right_cols = 0;
    }
    if (VGA_HEIGHT <= (pad_top_rows + pad_bottom_rows)) {
        pad_top_rows = 0;
        pad_bottom_rows = 0;
    }
    screen_cols_value = VGA_WIDTH - pad_left_cols - pad_right_cols;
    screen_rows_value = VGA_HEIGHT - pad_top_rows - pad_bottom_rows;
    if (screen_cols_value < 1) screen_cols_value = 1;
    if (screen_rows_value < 1) screen_rows_value = 1;

    fb_addr = 0;
    fb_pitch = 0;
    fb_width = 0;
    fb_height = 0;
    fb_bpp = 0;
    fb_bytes_per_pixel = 0;
    fb_type = 0;
    fb_origin_x = 0;
    fb_origin_y = 0;
    fb_font = (font_t){
        .width = 0,
        .height = 0,
        .row_bytes = 0,
        .glyph_count = 0,
        .bytes_per_glyph = 0,
        .glyphs = NULL,
        .data = NULL,
        .data_len = 0,
        .headersize = 0,
        .flags = 0,
    };

    if (multiboot_magic == MULTIBOOT_BOOTLOADER_MAGIC && mboot_info) {
        const multiboot_info_t* mbi = (const multiboot_info_t*)mboot_info;
        if (mbi->flags & (1u << 12)) {
            if (mbi->framebuffer_type == 1 && (mbi->framebuffer_bpp == 32 || mbi->framebuffer_bpp == 24 || mbi->framebuffer_bpp == 16)) {
                uint32_t addr_high = mbi->framebuffer_addr_high;
                uint32_t addr_low = mbi->framebuffer_addr_low;
                if (addr_high == 0 && addr_low != 0) {
                    fb_addr = (uint8_t*)addr_low;
                    fb_pitch = mbi->framebuffer_pitch;
                    fb_width = mbi->framebuffer_width;
                    fb_height = mbi->framebuffer_height;
                    fb_bpp = mbi->framebuffer_bpp;
                    fb_bytes_per_pixel = (uint8_t)((fb_bpp + 7u) / 8u);
                    fb_type = mbi->framebuffer_type;
                    fb_r_pos = mbi->framebuffer_red_field_position;
                    fb_r_size = mbi->framebuffer_red_mask_size;
                    fb_g_pos = mbi->framebuffer_green_field_position;
                    fb_g_size = mbi->framebuffer_green_mask_size;
                    fb_b_pos = mbi->framebuffer_blue_field_position;
                    fb_b_size = mbi->framebuffer_blue_mask_size;

                    // Prefer larger, high-quality framebuffer fonts by default.
                    const int* candidates = NULL;
                    int candidates_count = 0;

                    const int large_candidates[] = {
                        FB_FONT_TERMINUS_BOLD_32X16,
                        FB_FONT_TERMINUS_32X16,
                        FB_FONT_SPLEEN_16X32,
                        FB_FONT_VGA_32X16,
                    };
                    const int small_candidates[] = {
                        FB_FONT_TERMINUS_24X12,
                        FB_FONT_SPLEEN_12X24,
                        FB_FONT_VGA_28X16,
                    };

                    if (fb_width >= 1024 && fb_height >= 768) {
                        candidates = large_candidates;
                        candidates_count = (int)(sizeof(large_candidates) / sizeof(large_candidates[0]));
                    } else {
                        candidates = small_candidates;
                        candidates_count = (int)(sizeof(small_candidates) / sizeof(small_candidates[0]));
                    }

                    bool font_ok = false;
                    fb_font_current_index = -1;
                    for (int i = 0; i < candidates_count; i++) {
                        const int idx = candidates[i];
                        const uint8_t* font_data = NULL;
                        uint32_t font_len = 0;
                        if (!fb_font_source_get(idx, NULL, &font_data, &font_len)) {
                            continue;
                        }
                        if (!font_psf2_parse(font_data, font_len, &fb_font)) {
                            continue;
                        }
                        if (fb_font.width == 0 || fb_font.height == 0) {
                            continue;
                        }
                        if ((fb_width / fb_font.width) < 1u || (fb_height / fb_font.height) < 1u) {
                            continue;
                        }
                        font_ok = true;
                        fb_font_current_index = idx;
                        break;
                    }

                    if (!font_ok || fb_font.width == 0 || fb_font.height == 0) {
                        serial_write_string("[WARN] Framebuffer font unavailable, using VGA text\n");
                        fb_font_current_index = -1;
                        fb_addr = 0;
                        fb_pitch = 0;
                        fb_width = 0;
                        fb_height = 0;
                        fb_bpp = 0;
                        fb_bytes_per_pixel = 0;
                        fb_type = 0;
                    } else {
                        int cols_total = (int)(fb_width / fb_font.width);
                        int rows_total = (int)(fb_height / fb_font.height);
                        int fb_pad_left = 1;
                        int fb_pad_right = 1;
                        int fb_pad_top = 1;
                        int fb_pad_bottom = 1;

                        if (cols_total <= (fb_pad_left + fb_pad_right)) {
                            fb_pad_left = 0;
                            fb_pad_right = 0;
                        }
                        if (rows_total <= (fb_pad_top + fb_pad_bottom)) {
                            fb_pad_top = 0;
                            fb_pad_bottom = 0;
                        }

                        int cols = cols_total - fb_pad_left - fb_pad_right;
                        int rows = rows_total - fb_pad_top - fb_pad_bottom;
                        if (cols < 1) cols = 1;
                        if (rows < 1) rows = 1;
                        if (cols > FB_MAX_COLS) cols = FB_MAX_COLS;
                        if (rows > FB_MAX_ROWS) rows = FB_MAX_ROWS;

                        pad_left_cols = fb_pad_left;
                        pad_right_cols = fb_pad_right;
                        pad_top_rows = fb_pad_top;
                        pad_bottom_rows = fb_pad_bottom;
                        screen_cols_value = cols;
                        screen_rows_value = rows;
                        fb_origin_x = (uint32_t)pad_left_cols * fb_font.width;
                        fb_origin_y = (uint32_t)pad_top_rows * fb_font.height;
                        backend = SCREEN_BACKEND_FRAMEBUFFER;
                        fb_unicode_build_map();

                        serial_write_string("[OK] Framebuffer ");
                        serial_write_dec((int32_t)fb_width);
                        serial_write_char('x');
                        serial_write_dec((int32_t)fb_height);
                        serial_write_string("x");
                        serial_write_dec((int32_t)fb_bpp);
                        serial_write_string(" font ");
                        serial_write_dec((int32_t)fb_font.width);
                        serial_write_char('x');
                        serial_write_dec((int32_t)fb_font.height);
                        serial_write_string(" rgb ");
                        serial_write_dec((int32_t)fb_r_pos);
                        serial_write_char('/');
                        serial_write_dec((int32_t)fb_r_size);
                        serial_write_char(' ');
                        serial_write_dec((int32_t)fb_g_pos);
                        serial_write_char('/');
                        serial_write_dec((int32_t)fb_g_size);
                        serial_write_char(' ');
                        serial_write_dec((int32_t)fb_b_pos);
                        serial_write_char('/');
                        serial_write_dec((int32_t)fb_b_size);
                        serial_write_char('\n');
                    }
                }
            }
        }
    }

    scrollback_reset();
    ansi_scroll_region_reset();
    screen_clear();
}

void screen_mouse_set_enabled(bool enabled) {
    mouse_cursor_enabled = enabled;
    fb_update_mouse_cursor();
}

void screen_mouse_set_pos(int x, int y) {
    mouse_cursor_x = x;
    mouse_cursor_y = y;
    if (mouse_cursor_x < 0) mouse_cursor_x = 0;
    if (mouse_cursor_y < 0) mouse_cursor_y = 0;
    if (mouse_cursor_x >= screen_cols_value) mouse_cursor_x = screen_cols_value - 1;
    int height = usable_height();
    if (height < 1) height = 1;
    if (mouse_cursor_y >= height) mouse_cursor_y = height - 1;
    fb_update_mouse_cursor();
}

bool screen_vt_mouse_reporting_enabled(void) {
    return vt_mouse_mode_mask != 0;
}

bool screen_vt_mouse_reporting_sgr(void) {
    return vt_mouse_sgr;
}

bool screen_vt_mouse_reporting_wheel(void) {
    return (vt_mouse_mode_mask & (VT_MOUSE_MODE_1002 | VT_MOUSE_MODE_1003)) != 0;
}

static uint8_t screen_glyph_for_codepoint(uint32_t cp) {
    uint8_t glyph = (uint8_t)'?';

    if (backend == SCREEN_BACKEND_FRAMEBUFFER && fb_unicode_ready) {
        if (fb_unicode_lookup(cp, &glyph)) {
            return glyph;
        }
        if (cp < fb_font.glyph_count) {
            return (uint8_t)cp;
        }
        return fb_unicode_replacement_glyph;
    }

    if (cp < 256u) {
        return (uint8_t)cp;
    }
    return (uint8_t)'?';
}

static void screen_put_codepoint(uint32_t cp) {
    int height = usable_height();
    bool render_now = !(backend == SCREEN_BACKEND_FRAMEBUFFER && scrollback_view_offset > 0);

    if (cp == (uint32_t)'\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (cp == (uint32_t)'\r') {
        cursor_x = 0;
    } else if (cp == (uint32_t)'\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (cp == (uint32_t)'\b') {
        if (cursor_x > 0) {
            cursor_x--;
            if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
                fb_cells[cursor_y * screen_cols_value + cursor_x] = fb_cell_make((uint8_t)' ', current_fg, current_bg);
                if (render_now) {
                    fb_render_cell(cursor_x, cursor_y);
                }
            } else {
                VGA_BUFFER[screen_phys_y(cursor_y) * VGA_WIDTH + screen_phys_x(cursor_x)] = vga_entry(' ', current_color);
            }
        }
    } else {
        uint8_t glyph = screen_glyph_for_codepoint(cp);
        if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
            fb_cells[cursor_y * screen_cols_value + cursor_x] = fb_cell_make(glyph, current_fg, current_bg);
            if (render_now) {
                fb_render_cell(cursor_x, cursor_y);
            }
        } else {
            VGA_BUFFER[screen_phys_y(cursor_y) * VGA_WIDTH + screen_phys_x(cursor_x)] = vga_entry((char)glyph, current_color);
        }
        cursor_x++;
    }

    if (cursor_x >= screen_cols_value) {
        cursor_x = 0;
        cursor_y++;
    }

    ansi_scroll_region_clamp();
    if (cursor_y > ansi_scroll_bottom || cursor_y >= height) {
        if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
            fb_scroll();
        } else {
            vga_scroll();
        }
    }

    update_cursor();
}

static void screen_utf8_process_byte(uint8_t b) {
    for (;;) {
        if (utf8_state.remaining == 0) {
            if (b < 0x80u) {
                screen_put_codepoint((uint32_t)b);
                return;
            }

            if (b >= 0xC2u && b <= 0xDFu) {
                utf8_state.codepoint = (uint32_t)(b & 0x1Fu);
                utf8_state.min = 0x80u;
                utf8_state.remaining = 1;
                return;
            }
            if (b >= 0xE0u && b <= 0xEFu) {
                utf8_state.codepoint = (uint32_t)(b & 0x0Fu);
                utf8_state.min = 0x800u;
                utf8_state.remaining = 2;
                return;
            }
            if (b >= 0xF0u && b <= 0xF4u) {
                utf8_state.codepoint = (uint32_t)(b & 0x07u);
                utf8_state.min = 0x10000u;
                utf8_state.remaining = 3;
                return;
            }

            // Invalid start: treat as a single-byte codepoint.
            screen_put_codepoint((uint32_t)b);
            return;
        }

        if ((b & 0xC0u) != 0x80u) {
            // Broken sequence: emit replacement and restart with this byte.
            utf8_state = (utf8_state_t){0};
            screen_put_codepoint(0xFFFDu);
            continue;
        }

        utf8_state.codepoint = (utf8_state.codepoint << 6) | (uint32_t)(b & 0x3Fu);
        utf8_state.remaining--;

        if (utf8_state.remaining != 0) {
            return;
        }

        uint32_t cp = utf8_state.codepoint;
        uint32_t min = utf8_state.min;
        utf8_state = (utf8_state_t){0};

        if (cp < min || cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
            cp = 0xFFFDu;
        }

        screen_put_codepoint(cp);
        return;
    }
}

void screen_putchar(char c) {
    if (ansi_handle_char(c)) {
        // Mirror ANSI escape bytes to serial so VT100-style userland (microrl, etc.)
        // remains usable over a host terminal connected to COM1.
        utf8_state = (utf8_state_t){0};
        serial_write_char(c);
        return;
    }

    screen_utf8_process_byte((uint8_t)c);

    // Mirror VGA output to serial for debugging/logging.
    serial_write_char(c);
}

void screen_print(const char* str) {
    while (*str) {
        screen_putchar(*str++);
    }
}

void screen_println(const char* str) {
    screen_print(str);
    screen_putchar('\n');
}

void screen_print_hex(uint32_t num) {
    screen_print("0x");
    char hex_chars[] = "0123456789ABCDEF";
    bool leading = true;

    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (num >> i) & 0xF;
        if (nibble != 0 || !leading || i == 0) {
            screen_putchar(hex_chars[nibble]);
            leading = false;
        }
    }
}

void screen_print_dec(int32_t num) {
    if (num < 0) {
        screen_putchar('-');
        num = -num;
    }

    if (num == 0) {
        screen_putchar('0');
        return;
    }

    char buffer[12];
    int i = 0;

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i > 0) {
        screen_putchar(buffer[--i]);
    }
}

void screen_set_color(uint8_t fg, uint8_t bg) {
    default_fg = (uint8_t)(fg & 0x0Fu);
    default_bg = (uint8_t)(bg & 0x0Fu);
    current_fg = default_fg;
    current_bg = default_bg;
    update_vga_colors();
}

void screen_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    update_cursor();
}

int screen_get_cursor_x(void) {
    return cursor_x;
}

int screen_get_cursor_y(void) {
    return cursor_y;
}

void screen_backspace(void) {
    if (cursor_x > 0) {
        cursor_x--;
        if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
            fb_cells[cursor_y * screen_cols_value + cursor_x] = fb_cell_make((uint8_t)' ', current_fg, current_bg);
            fb_render_cell(cursor_x, cursor_y);
        } else {
            VGA_BUFFER[screen_phys_y(cursor_y) * VGA_WIDTH + screen_phys_x(cursor_x)] = vga_entry(' ', current_color);
        }
        update_cursor();
    }
}

void screen_set_reserved_bottom_rows(int rows) {
    if (rows < 0) {
        rows = 0;
    }
    if (rows >= screen_rows_value) {
        rows = screen_rows_value - 1;
    }
    reserved_bottom_rows = rows;
    ansi_scroll_region_clamp();
    if (cursor_y >= usable_height()) {
        cursor_y = usable_height() - 1;
        if (cursor_y < 0) cursor_y = 0;
        if (cursor_x >= screen_cols_value) cursor_x = 0;
        update_cursor();
    }
}

void screen_write_char_at(int x, int y, char c, uint8_t color) {
    if (x < 0 || x >= screen_cols_value || y < 0 || y >= screen_rows_value) {
        return;
    }
    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        uint8_t fg = (uint8_t)(color & 0x0Fu);
        uint8_t bg = (uint8_t)((color >> 4) & 0x0Fu);
        fb_cells[y * screen_cols_value + x] = fb_cell_make((uint8_t)c, fg, bg);
        fb_render_cell(x, y);
        if (cursor_drawn_x == x && cursor_drawn_y == y) {
            cursor_drawn_x = -1;
            cursor_drawn_y = -1;
            update_cursor();
        }
    } else {
        VGA_BUFFER[screen_phys_y(y) * VGA_WIDTH + screen_phys_x(x)] = vga_entry(c, color);
    }
}

void screen_write_string_at(int x, int y, const char* str, uint8_t color) {
    if (!str || y < 0 || y >= screen_rows_value) {
        return;
    }
    int col = x;
    while (*str && col < screen_cols_value) {
        if (col >= 0) {
            screen_write_char_at(col, y, *str, color);
        }
        col++;
        str++;
    }
}

void screen_fill_row(int y, char c, uint8_t color) {
    if (y < 0 || y >= screen_rows_value) {
        return;
    }
    for (int x = 0; x < screen_cols_value; x++) {
        screen_write_char_at(x, y, c, color);
    }
}

void screen_cursor_set_enabled(bool enabled) {
    cursor_enabled = enabled;
    update_cursor();
}

bool screen_is_framebuffer(void) {
    return backend == SCREEN_BACKEND_FRAMEBUFFER;
}

uint32_t screen_framebuffer_width(void) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return 0;
    }
    return fb_width;
}

uint32_t screen_framebuffer_height(void) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return 0;
    }
    return fb_height;
}

uint32_t screen_framebuffer_bpp(void) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return 0;
    }
    return fb_bpp;
}

uint32_t screen_font_width(void) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return 0;
    }
    return fb_font.width;
}

uint32_t screen_font_height(void) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return 0;
    }
    return fb_font.height;
}

int screen_font_count(void) {
    return fb_font_count_value();
}

int screen_font_get_current(void) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return -ENOTTY;
    }
    return fb_font_current_index;
}

int screen_font_get_info(int index, screen_font_info_t* out) {
    if (!out) {
        return -EINVAL;
    }

    const fb_font_source_t* src = NULL;
    const uint8_t* data = NULL;
    uint32_t len = 0;
    if (!fb_font_source_get(index, &src, &data, &len)) {
        return -EINVAL;
    }

    font_t parsed;
    if (!font_psf2_parse(data, len, &parsed)) {
        return -EINVAL;
    }

    memset(out, 0, sizeof(*out));
    strncpy(out->name, src->name, sizeof(out->name) - 1u);
    out->width = parsed.width;
    out->height = parsed.height;
    return 0;
}

static int fb_apply_font(const font_t* font) {
    if (!font) {
        return -EINVAL;
    }
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return -ENOTTY;
    }
    if (font->width == 0 || font->height == 0) {
        return -EINVAL;
    }

    int cols_total = (int)(fb_width / font->width);
    int rows_total = (int)(fb_height / font->height);
    if (cols_total < 1 || rows_total < 1) {
        return -EINVAL;
    }

    int fb_pad_left = 1;
    int fb_pad_right = 1;
    int fb_pad_top = 1;
    int fb_pad_bottom = 1;

    if (cols_total <= (fb_pad_left + fb_pad_right)) {
        fb_pad_left = 0;
        fb_pad_right = 0;
    }
    if (rows_total <= (fb_pad_top + fb_pad_bottom)) {
        fb_pad_top = 0;
        fb_pad_bottom = 0;
    }

    int cols = cols_total - fb_pad_left - fb_pad_right;
    int rows = rows_total - fb_pad_top - fb_pad_bottom;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (cols > FB_MAX_COLS) cols = FB_MAX_COLS;
    if (rows > FB_MAX_ROWS) rows = FB_MAX_ROWS;

    fb_font = *font;
    pad_left_cols = fb_pad_left;
    pad_right_cols = fb_pad_right;
    pad_top_rows = fb_pad_top;
    pad_bottom_rows = fb_pad_bottom;
    screen_cols_value = cols;
    screen_rows_value = rows;
    fb_origin_x = (uint32_t)pad_left_cols * fb_font.width;
    fb_origin_y = (uint32_t)pad_top_rows * fb_font.height;

    // Keep the status bar reservation if possible.
    screen_set_reserved_bottom_rows(reserved_bottom_rows);
    scrollback_reset();
    ansi_scroll_region_reset();
    fb_unicode_build_map();
    screen_clear();
    statusbar_refresh();
    return 0;
}

int screen_font_set(int index) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return -ENOTTY;
    }

    const uint8_t* data = NULL;
    uint32_t len = 0;
    if (!fb_font_source_get(index, NULL, &data, &len)) {
        return -EINVAL;
    }

    font_t parsed;
    if (!font_psf2_parse(data, len, &parsed)) {
        return -EINVAL;
    }

    uint32_t flags = irq_save();
    int rc = fb_apply_font(&parsed);
    if (rc == 0) {
        fb_font_current_index = index;
    }
    irq_restore(flags);
    return rc;
}

static inline bool fb_xy_in_bounds(int32_t x, int32_t y) {
    return x >= 0 && y >= 0 && (uint32_t)x < fb_width && (uint32_t)y < fb_height;
}

bool screen_graphics_clear(uint8_t bg_vga) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return false;
    }
    uint32_t px = fb_color_from_xterm(bg_vga);
    fb_fill_rect(0, 0, fb_width, fb_height, px);
    return true;
}

bool screen_graphics_putpixel(int32_t x, int32_t y, uint8_t vga_color) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return false;
    }
    if (!fb_xy_in_bounds(x, y)) {
        return false;
    }
    uint32_t px = fb_color_from_xterm(vga_color);
    fb_put_pixel((uint32_t)x, (uint32_t)y, px);
    return true;
}

bool screen_graphics_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t vga_color) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return false;
    }

    uint32_t px = fb_color_from_xterm(vga_color);

    int32_t dx = x1 - x0;
    int32_t sx = (dx >= 0) ? 1 : -1;
    if (dx < 0) dx = -dx;

    int32_t dy = y1 - y0;
    int32_t sy = (dy >= 0) ? 1 : -1;
    if (dy < 0) dy = -dy;

    int32_t err = (dx > dy ? dx : -dy) / 2;

    for (;;) {
        if (fb_xy_in_bounds(x0, y0)) {
            fb_put_pixel((uint32_t)x0, (uint32_t)y0, px);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int32_t e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }

    return true;
}

bool screen_graphics_blit_rgba(int32_t x, int32_t y, uint32_t w, uint32_t h, const uint8_t* rgba, uint32_t stride_bytes) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return false;
    }
    if (!rgba || w == 0 || h == 0) {
        return false;
    }
    if (stride_bytes < w * 4u) {
        return false;
    }
    if (x < 0 || y < 0) {
        return false;
    }
    if ((uint32_t)x + w > fb_width || (uint32_t)y + h > fb_height) {
        return false;
    }

    for (uint32_t yy = 0; yy < h; yy++) {
        const uint8_t* src = rgba + yy * stride_bytes;
        uint8_t* dst = fb_addr + ((uint32_t)y + yy) * fb_pitch + (uint32_t)x * (uint32_t)fb_bytes_per_pixel;

        switch (fb_bytes_per_pixel) {
            case 4: {
                uint32_t* out = (uint32_t*)dst;
                for (uint32_t xx = 0; xx < w; xx++) {
                    uint8_t r = src[xx * 4u + 0u];
                    uint8_t g = src[xx * 4u + 1u];
                    uint8_t b = src[xx * 4u + 2u];
                    out[xx] = fb_pack_rgb(r, g, b);
                }
                break;
            }
            case 3: {
                for (uint32_t xx = 0; xx < w; xx++) {
                    uint8_t r = src[xx * 4u + 0u];
                    uint8_t g = src[xx * 4u + 1u];
                    uint8_t b = src[xx * 4u + 2u];
                    uint32_t px = fb_pack_rgb(r, g, b);
                    dst[xx * 3u + 0u] = (uint8_t)(px & 0xFFu);
                    dst[xx * 3u + 1u] = (uint8_t)((px >> 8) & 0xFFu);
                    dst[xx * 3u + 2u] = (uint8_t)((px >> 16) & 0xFFu);
                }
                break;
            }
            case 2: {
                uint16_t* out = (uint16_t*)dst;
                for (uint32_t xx = 0; xx < w; xx++) {
                    uint8_t r = src[xx * 4u + 0u];
                    uint8_t g = src[xx * 4u + 1u];
                    uint8_t b = src[xx * 4u + 2u];
                    out[xx] = (uint16_t)(fb_pack_rgb(r, g, b) & 0xFFFFu);
                }
                break;
            }
            default: {
                for (uint32_t xx = 0; xx < w; xx++) {
                    uint8_t r = src[xx * 4u + 0u];
                    uint8_t g = src[xx * 4u + 1u];
                    uint8_t b = src[xx * 4u + 2u];
                    dst[xx] = (uint8_t)(fb_pack_rgb(r, g, b) & 0xFFu);
                }
                break;
            }
        }
    }

    return true;
}
