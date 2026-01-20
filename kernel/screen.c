#include "screen.h"
#include "io.h"
#include "serial.h"
#include "string.h"
#include "multiboot.h"
#include "font.h"
#include "font_terminus_psf2.h"
#include "font_vga_psf2.h"

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

// Current color attribute (VGA-style: fg | (bg << 4))
static uint8_t current_color = 0x0F; // White on black
static uint8_t default_color = 0x0F;

static int reserved_bottom_rows = 0;

// Minimal ANSI/VT100 parsing (CSI sequences) used by some vendored CLI code.
typedef enum {
    ANSI_STATE_NONE = 0,
    ANSI_STATE_ESC,
    ANSI_STATE_CSI,
} ansi_state_t;

static ansi_state_t ansi_state = ANSI_STATE_NONE;
static int ansi_params[8];
static int ansi_param_count = 0;
static int ansi_current = -1;
static bool ansi_private = false;
static int ansi_saved_x = 0;
static int ansi_saved_y = 0;

static inline int screen_phys_x(int x);
static inline int screen_phys_y(int y);
static void fb_render_cell(int x, int y);
static void update_cursor(void);

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

static uint16_t fb_cells[FB_MAX_COLS * FB_MAX_ROWS];

#define SCROLLBACK_MAX_LINES 1024
static uint16_t scrollback_cells[SCROLLBACK_MAX_LINES * FB_MAX_COLS];
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

static void scrollback_reset(void) {
    scrollback_head = 0;
    scrollback_count = 0;
    scrollback_view_offset = 0;
    scrollback_cols = screen_cols_value;
    cursor_force_hidden = false;
}

static uint16_t* scrollback_line_ptr(uint32_t idx) {
    uint32_t real = (scrollback_head + idx) % SCROLLBACK_MAX_LINES;
    return &scrollback_cells[real * FB_MAX_COLS];
}

static void scrollback_push_line(const uint16_t* line, int cols) {
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

    uint16_t* dst = 0;
    if (scrollback_count < SCROLLBACK_MAX_LINES) {
        dst = scrollback_line_ptr(scrollback_count);
        scrollback_count++;
    } else {
        dst = &scrollback_cells[scrollback_head * FB_MAX_COLS];
        scrollback_head = (scrollback_head + 1) % SCROLLBACK_MAX_LINES;
    }

    memcpy(dst, line, (size_t)cols * sizeof(uint16_t));
    if (cols < FB_MAX_COLS) {
        uint16_t blank = vga_entry(' ', current_color);
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

    if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
        uint16_t blank = vga_entry(' ', current_color);
        uint16_t* row = &fb_cells[y * screen_cols_value];
        for (int x = cursor_x; x < screen_cols_value; x++) {
            row[x] = blank;
            fb_render_cell(x, y);
        }
    } else {
        for (int x = cursor_x; x < screen_cols_value; x++) {
            VGA_BUFFER[screen_phys_y(y) * VGA_WIDTH + screen_phys_x(x)] = vga_entry(' ', current_color);
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

static void ansi_apply_sgr_param(int p) {
    uint8_t fg = (uint8_t)(current_color & 0x0Fu);
    uint8_t bg = (uint8_t)((current_color >> 4) & 0x0Fu);
    uint8_t def_fg = (uint8_t)(default_color & 0x0Fu);
    uint8_t def_bg = (uint8_t)((default_color >> 4) & 0x0Fu);

    if (p == 0) {
        current_color = default_color;
        return;
    }
    if (p == 1) { // bold/bright
        if (fg < 8u) {
            fg = (uint8_t)(fg + 8u);
        }
        current_color = vga_color(fg, bg);
        return;
    }
    if (p == 22) { // normal intensity
        if (fg >= 8u) {
            fg = (uint8_t)(fg - 8u);
        }
        current_color = vga_color(fg, bg);
        return;
    }
    if (p == 7) { // reverse video
        current_color = vga_color(bg, fg);
        return;
    }
    if (p == 27) { // reverse off (best-effort)
        current_color = default_color;
        return;
    }

    if (p >= 30 && p <= 37) {
        fg = ansi_basic_to_vga((uint8_t)(p - 30));
        current_color = vga_color(fg, bg);
        return;
    }
    if (p >= 90 && p <= 97) {
        fg = (uint8_t)(ansi_basic_to_vga((uint8_t)(p - 90)) + 8u);
        current_color = vga_color(fg, bg);
        return;
    }
    if (p >= 40 && p <= 47) {
        bg = ansi_basic_to_vga((uint8_t)(p - 40));
        current_color = vga_color(fg, bg);
        return;
    }
    if (p >= 100 && p <= 107) {
        bg = (uint8_t)(ansi_basic_to_vga((uint8_t)(p - 100)) + 8u);
        current_color = vga_color(fg, bg);
        return;
    }
    if (p == 39) { // default fg
        fg = def_fg;
        current_color = vga_color(fg, bg);
        return;
    }
    if (p == 49) { // default bg
        bg = def_bg;
        current_color = vga_color(fg, bg);
        return;
    }
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
            ansi_erase_to_eol();
            update_cursor();
            break;
        }
        case 'J': { // erase in display (2 = clear)
            int mode = ansi_get_param(0, 0);
            if (mode == 2) {
                screen_clear();
                update_cursor();
            }
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
            if (ansi_param_count == 0) {
                ansi_apply_sgr_param(0);
                break;
            }
            for (int i = 0; i < ansi_param_count; i++) {
                ansi_apply_sgr_param(ansi_params[i]);
            }
            break;
        }
        case 'h':
        case 'l': {
            if (ansi_private) {
                bool set = (c == 'h');
                for (int i = 0; i < ansi_param_count; i++) {
                    if (ansi_params[i] == 25) {
                        cursor_vt_hidden = !set;
                        update_cursor();
                        break;
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

static void fb_render_entry(int x, int y, uint16_t entry) {
    if (x < 0 || y < 0 || x >= screen_cols_value || y >= screen_rows_value) {
        return;
    }

    uint8_t ch = (uint8_t)(entry & 0xFFu);
    uint8_t color = (uint8_t)((entry >> 8) & 0xFFu);
    uint8_t fg = (uint8_t)(color & 0x0Fu);
    uint8_t bg = (uint8_t)((color >> 4) & 0x0Fu);

    uint32_t fg_px = fb_color_from_vga(fg);
    uint32_t bg_px = fb_color_from_vga(bg);

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

    uint16_t entry = fb_cells[y * screen_cols_value + x];
    uint8_t color = (uint8_t)((entry >> 8) & 0xFFu);
    uint8_t fg = (uint8_t)(color & 0x0Fu);
    uint32_t fg_px = fb_color_from_vga(fg);

    uint32_t base_x = fb_origin_x + (uint32_t)x * fb_font.width;
    uint32_t base_y = fb_origin_y + (uint32_t)y * fb_font.height;
    uint32_t thickness = fb_cursor_thickness();
    uint32_t y0 = base_y + (fb_font.height - thickness);
    fb_fill_rect(base_x, y0, fb_font.width, thickness, fg_px);
}

static void fb_update_cursor(void) {
    if (cursor_force_hidden || cursor_vt_hidden || !cursor_enabled) {
        if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
            fb_render_cell(cursor_drawn_x, cursor_drawn_y);
            cursor_drawn_x = -1;
            cursor_drawn_y = -1;
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
    int height = usable_height();
    int phys_top = pad_top_rows;

    // Save the line that is about to scroll out (top visible line).
    int cols = screen_cols_value;
    if (cols > FB_MAX_COLS) cols = FB_MAX_COLS;
    if (cols > 0) {
        uint16_t line[FB_MAX_COLS];
        for (int x = 0; x < cols; x++) {
            line[x] = VGA_BUFFER[screen_phys_y(0) * VGA_WIDTH + screen_phys_x(x)];
        }
        scrollback_push_line(line, cols);
    }

    // Move all lines up by one
    for (int y = 0; y < height - 1; y++) {
        int dst_y = phys_top + y;
        int src_y = phys_top + y + 1;
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[dst_y * VGA_WIDTH + x] = VGA_BUFFER[src_y * VGA_WIDTH + x];
        }
    }

    // Clear the last line
    int last_y = phys_top + (height - 1);
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[last_y * VGA_WIDTH + x] = vga_entry(' ', current_color);
    }

    cursor_y = height - 1;
}

static void fb_scroll(void) {
    int height = usable_height();
    if (height <= 1) {
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

    int cols = screen_cols_value;
    if (cols > 0) {
        scrollback_push_line(&fb_cells[0], cols);
    }
    size_t row_bytes = (size_t)cols * sizeof(uint16_t);
    memcpy(&fb_cells[0], &fb_cells[cols], row_bytes * (size_t)(height - 1));

    uint16_t blank = vga_entry(' ', current_color);
    for (int x = 0; x < cols; x++) {
        fb_cells[(height - 1) * cols + x] = blank;
    }

    if (scrollback_view_offset == 0) {
        uint32_t usable_px_height = (uint32_t)height * fb_font.height;
        uint32_t copy_bytes = (usable_px_height - fb_font.height) * fb_pitch;
        uint8_t* dst = fb_addr + fb_origin_y * fb_pitch;
        uint8_t* src = dst + fb_font.height * fb_pitch;
        memcpy(dst, src, copy_bytes);

        uint8_t bg = (uint8_t)((current_color >> 4) & 0x0Fu);
        uint32_t bg_px = fb_color_from_vga(bg);
        uint32_t clear_y = fb_origin_y + (uint32_t)(height - 1) * fb_font.height;
        fb_fill_rect(0, clear_y, fb_width, fb_font.height, bg_px);
    }

    cursor_y = height - 1;
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
        const uint16_t* src = 0;
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
            uint16_t entry = is_history ? src[x] : src[x];
            fb_render_entry(x, y, entry);
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
        uint16_t blank = vga_entry(' ', current_color);
        int cols = screen_cols_value;
        int rows = screen_rows_value;

        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                fb_cells[y * cols + x] = blank;
            }
        }

        uint8_t bg = (uint8_t)((current_color >> 4) & 0x0Fu);
        uint32_t bg_px = fb_color_from_vga(bg);
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
}

void screen_init(uint32_t multiboot_magic, uint32_t* mboot_info) {
    current_color = vga_color(VGA_WHITE, VGA_BLUE);
    reserved_bottom_rows = 0;
    cursor_x = 0;
    cursor_y = 0;
    cursor_enabled = true;
    cursor_drawn_x = -1;
    cursor_drawn_y = -1;

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

                    const uint8_t* font_data = font_vga28x16_psf2;
                    uint32_t font_len = font_vga28x16_psf2_len;
                    const uint8_t* fallback_font_data = font_terminus24x12_psf2;
                    uint32_t fallback_font_len = font_terminus24x12_psf2_len;
                    if (fb_width >= 1024 && fb_height >= 768) {
                        font_data = font_vga32x16_psf2;
                        font_len = font_vga32x16_psf2_len;
                        fallback_font_data = font_terminus32x16_psf2;
                        fallback_font_len = font_terminus32x16_psf2_len;
                    }

                    bool font_ok = font_psf2_parse(font_data, font_len, &fb_font);
                    if (!font_ok) {
                        font_ok = font_psf2_parse(fallback_font_data, fallback_font_len, &fb_font);
                    }

                    if (!font_ok || fb_font.width == 0 || fb_font.height == 0) {
                        serial_write_string("[WARN] Framebuffer font unavailable, using VGA text\n");
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
    screen_clear();
}

void screen_putchar(char c) {
    if (ansi_handle_char(c)) {
        // Mirror ANSI escape bytes to serial so VT100-style userland (microrl, etc.)
        // remains usable over a host terminal connected to COM1.
        serial_write_char(c);
        return;
    }

    int height = usable_height();
    bool render_now = !(backend == SCREEN_BACKEND_FRAMEBUFFER && scrollback_view_offset > 0);

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
                fb_cells[cursor_y * screen_cols_value + cursor_x] = vga_entry(' ', current_color);
                if (render_now) {
                    fb_render_cell(cursor_x, cursor_y);
                }
            } else {
                VGA_BUFFER[screen_phys_y(cursor_y) * VGA_WIDTH + screen_phys_x(cursor_x)] = vga_entry(' ', current_color);
            }
        }
    } else {
        if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
            fb_cells[cursor_y * screen_cols_value + cursor_x] = vga_entry(c, current_color);
            if (render_now) {
                fb_render_cell(cursor_x, cursor_y);
            }
        } else {
            VGA_BUFFER[screen_phys_y(cursor_y) * VGA_WIDTH + screen_phys_x(cursor_x)] = vga_entry(c, current_color);
        }
        cursor_x++;
    }

    // Handle line wrap
    if (cursor_x >= screen_cols_value) {
        cursor_x = 0;
        cursor_y++;
    }

    // Handle scrolling
    if (cursor_y >= height) {
        if (backend == SCREEN_BACKEND_FRAMEBUFFER) {
            fb_scroll();
        } else {
            vga_scroll();
        }
    }

    update_cursor();

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
    default_color = vga_color(fg, bg);
    current_color = default_color;
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
            fb_cells[cursor_y * screen_cols_value + cursor_x] = vga_entry(' ', current_color);
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
        fb_cells[y * screen_cols_value + x] = vga_entry(c, color);
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

static inline bool fb_xy_in_bounds(int32_t x, int32_t y) {
    return x >= 0 && y >= 0 && (uint32_t)x < fb_width && (uint32_t)y < fb_height;
}

bool screen_graphics_clear(uint8_t bg_vga) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return false;
    }
    uint32_t px = fb_color_from_vga(bg_vga);
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
    uint32_t px = fb_color_from_vga(vga_color);
    fb_put_pixel((uint32_t)x, (uint32_t)y, px);
    return true;
}

bool screen_graphics_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t vga_color) {
    if (backend != SCREEN_BACKEND_FRAMEBUFFER) {
        return false;
    }

    uint32_t px = fb_color_from_vga(vga_color);

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
