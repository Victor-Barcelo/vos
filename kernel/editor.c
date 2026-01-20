#include "editor.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"
#include "kheap.h"
#include "ramfs.h"

#define EDIT_MAX_LINES 512
#define EDIT_MAX_LINE_LEN 512
#define EDIT_STATUS_LEN 80

typedef struct edit_line {
    char* data;
    uint32_t len;
    uint32_t cap;
} edit_line_t;

static bool ci_eq(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }
    for (;;) {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) {
            return false;
        }
        if (ca == '\0') {
            return true;
        }
    }
}

static bool is_ram_abs(const char* abs_path) {
    if (!abs_path || abs_path[0] != '/') {
        return false;
    }
    const char* p = abs_path;
    while (*p == '/') p++;
    if (ci_eq(p, "ram")) {
        return true;
    }
    return (p[0] == 'r' || p[0] == 'R') &&
           (p[1] == 'a' || p[1] == 'A') &&
           (p[2] == 'm' || p[2] == 'M') &&
           p[3] == '/';
}

static bool ensure_line_cap(edit_line_t* line, uint32_t need) {
    if (!line) {
        return false;
    }
    if (need <= line->cap) {
        return true;
    }

    uint32_t new_cap = line->cap ? line->cap : 16u;
    while (new_cap < need) {
        uint32_t next = new_cap * 2u;
        if (next <= new_cap) {
            return false;
        }
        new_cap = next;
    }
    if (new_cap > EDIT_MAX_LINE_LEN + 1u) {
        new_cap = EDIT_MAX_LINE_LEN + 1u;
    }
    if (need > new_cap) {
        return false;
    }

    char* n = (char*)kmalloc(new_cap);
    if (!n) {
        return false;
    }
    if (line->data && line->len) {
        memcpy(n, line->data, line->len);
    }
    if (line->data) {
        kfree(line->data);
    }
    line->data = n;
    line->cap = new_cap;
    return true;
}

static bool line_insert_char(edit_line_t* line, uint32_t col, char c) {
    if (!line) {
        return false;
    }
    if (col > line->len) {
        col = line->len;
    }
    if (line->len >= EDIT_MAX_LINE_LEN) {
        return false;
    }
    if (!ensure_line_cap(line, line->len + 2u)) {
        return false;
    }
    memmove(line->data + col + 1u, line->data + col, line->len - col);
    line->data[col] = c;
    line->len++;
    line->data[line->len] = '\0';
    return true;
}

static bool line_delete_char(edit_line_t* line, uint32_t col) {
    if (!line) {
        return false;
    }
    if (line->len == 0 || col >= line->len) {
        return false;
    }
    memmove(line->data + col, line->data + col + 1u, line->len - col - 1u);
    line->len--;
    line->data[line->len] = '\0';
    return true;
}

static bool line_append(edit_line_t* dst, const edit_line_t* src) {
    if (!dst || !src) {
        return false;
    }
    if (dst->len + src->len > EDIT_MAX_LINE_LEN) {
        return false;
    }
    if (!ensure_line_cap(dst, dst->len + src->len + 1u)) {
        return false;
    }
    memcpy(dst->data + dst->len, src->data, src->len);
    dst->len += src->len;
    dst->data[dst->len] = '\0';
    return true;
}

static void fill_row(int y, uint8_t color) {
    screen_fill_row(y, ' ', color);
}

static void write_row(int x, int y, const char* s, uint8_t color) {
    screen_write_string_at(x, y, s, color);
}

static void set_cursor_pos(int x, int y) {
    screen_set_cursor(x, y);
}

static uint8_t color_shell(void) {
    return (uint8_t)(VGA_WHITE | (VGA_BLUE << 4));
}

static uint8_t color_header(void) {
    return (uint8_t)(VGA_BLACK | (VGA_LIGHT_GREY << 4));
}

static void clamp_u32(uint32_t* v, uint32_t max) {
    if (*v > max) {
        *v = max;
    }
}

static bool save_file(const char* abs_path, edit_line_t* lines, uint32_t line_count, char* status) {
    if (!abs_path || !lines || line_count == 0) {
        if (status) strcpy(status, "save failed");
        return false;
    }

    uint32_t total = 0;
    for (uint32_t i = 0; i < line_count; i++) {
        uint32_t add = lines[i].len;
        if (i + 1u < line_count) {
            add += 1u;
        }
        if (total + add < total) {
            if (status) strcpy(status, "save failed");
            return false;
        }
        total += add;
    }

    uint8_t* buf = NULL;
    if (total) {
        buf = (uint8_t*)kmalloc(total);
        if (!buf) {
            if (status) strcpy(status, "out of memory");
            return false;
        }
    }

    uint32_t pos = 0;
    for (uint32_t i = 0; i < line_count; i++) {
        if (lines[i].len) {
            memcpy(buf + pos, lines[i].data, lines[i].len);
            pos += lines[i].len;
        }
        if (i + 1u < line_count) {
            buf[pos++] = '\n';
        }
    }

    bool ok = ramfs_write_file(abs_path, buf, total, true);
    if (buf) {
        kfree(buf);
    }

    if (!ok) {
        if (status) strcpy(status, "save failed");
        return false;
    }

    if (status) {
        strcpy(status, "saved");
    }
    return true;
}

bool editor_nano(const char* abs_path) {
    if (!abs_path || !is_ram_abs(abs_path)) {
        return false;
    }

    // Load existing content (or create empty).
    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (!ramfs_read_file(abs_path, &data, &size)) {
        if (!ramfs_write_file(abs_path, NULL, 0, false)) {
            return false;
        }
        data = NULL;
        size = 0;
    }

    edit_line_t lines[EDIT_MAX_LINES];
    memset(lines, 0, sizeof(lines));
    uint32_t line_count = 0;

    if (data && size) {
        // Parse into lines, normalize CRLF -> LF.
        uint32_t i = 0;
        while (i < size && line_count < EDIT_MAX_LINES) {
            uint32_t start = i;
            while (i < size && data[i] != '\n') {
                i++;
            }
            uint32_t end = i;
            if (end > start && data[end - 1u] == '\r') {
                end--;
            }
            uint32_t len = end - start;
            if (len > EDIT_MAX_LINE_LEN) {
                len = EDIT_MAX_LINE_LEN;
            }

            edit_line_t* l = &lines[line_count];
            l->cap = len + 1u;
            l->data = (char*)kmalloc(l->cap);
            if (!l->data) {
                break;
            }
            if (len) {
                memcpy(l->data, data + start, len);
            }
            l->data[len] = '\0';
            l->len = len;
            line_count++;

            if (i < size && data[i] == '\n') {
                i++;
            }
        }
    }

    if (line_count == 0) {
        // Always have at least one line.
        lines[0].data = (char*)kmalloc(1u);
        if (!lines[0].data) {
            return false;
        }
        lines[0].data[0] = '\0';
        lines[0].len = 0;
        lines[0].cap = 1;
        line_count = 1;
    }

    uint32_t cur_line = 0;
    uint32_t cur_col = 0;
    uint32_t scroll_line = 0;
    uint32_t scroll_col = 0;
    bool modified = false;
    bool saved_once = false;
    bool exit_confirm = false;
    char status[EDIT_STATUS_LEN];
    status[0] = '\0';

    int cols = screen_cols();
    int rows = screen_rows();
    if (cols < 1) cols = 1;
    if (rows < 3) rows = 3;
    int text_rows = rows - 2; // header + statusbar
    if (text_rows < 1) text_rows = 1;

    uint8_t header_color = color_header();
    uint8_t text_color = color_shell();

    for (;;) {
        // Keep cursor in range.
        if (cur_line >= line_count) {
            cur_line = (line_count ? (line_count - 1u) : 0u);
        }
        if (line_count == 0) {
            line_count = 1;
        }
        clamp_u32(&cur_col, lines[cur_line].len);

        // Scroll vertical.
        if (cur_line < scroll_line) {
            scroll_line = cur_line;
        }
        if (cur_line >= scroll_line + (uint32_t)text_rows) {
            scroll_line = cur_line - (uint32_t)text_rows + 1u;
        }

        // Scroll horizontal.
        if (cur_col < scroll_col) {
            scroll_col = cur_col;
        }
        if (cur_col >= scroll_col + (uint32_t)cols) {
            scroll_col = cur_col - (uint32_t)cols + 1u;
        }

        // Draw header.
        fill_row(0, header_color);
        char hdr[256];
        hdr[0] = '\0';
        strcpy(hdr, " nano ");
        strncat(hdr, abs_path, sizeof(hdr) - strlen(hdr) - 1u);
        if (modified) {
            strncat(hdr, " [modified]", sizeof(hdr) - strlen(hdr) - 1u);
        }
        if (exit_confirm) {
            strncat(hdr, " (Ctrl+X again to quit)", sizeof(hdr) - strlen(hdr) - 1u);
        }
        if (status[0]) {
            strncat(hdr, " | ", sizeof(hdr) - strlen(hdr) - 1u);
            strncat(hdr, status, sizeof(hdr) - strlen(hdr) - 1u);
        } else {
            strncat(hdr, " | Ctrl+S save, Ctrl+X exit", sizeof(hdr) - strlen(hdr) - 1u);
        }
        write_row(0, 0, hdr, header_color);

        // Draw text area (rows 1..rows-2).
        for (int r = 0; r < text_rows; r++) {
            int y = 1 + r;
            fill_row(y, text_color);

            uint32_t li = scroll_line + (uint32_t)r;
            if (li >= line_count) {
                continue;
            }

            const edit_line_t* l = &lines[li];
            if (scroll_col >= l->len) {
                continue;
            }

            char row_buf[256];
            int max = cols;
            if (max > (int)sizeof(row_buf) - 1) {
                max = (int)sizeof(row_buf) - 1;
            }
            int n = 0;
            uint32_t start = scroll_col;
            while (n < max && start + (uint32_t)n < l->len) {
                char ch = l->data[start + (uint32_t)n];
                row_buf[n++] = (ch == '\t') ? ' ' : ch;
            }
            row_buf[n] = '\0';
            write_row(0, y, row_buf, text_color);
        }

        // Place cursor.
        int cx = (int)(cur_col - scroll_col);
        int cy = 1 + (int)(cur_line - scroll_line);
        if (cx < 0) cx = 0;
        if (cx >= cols) cx = cols - 1;
        if (cy < 1) cy = 1;
        if (cy >= rows - 1) cy = rows - 2;
        set_cursor_pos(cx, cy);

        char c = keyboard_getchar();
        status[0] = '\0';

        if (c == 24) { // Ctrl+X
            if (!modified || exit_confirm) {
                break;
            }
            exit_confirm = true;
            strcpy(status, "unsaved changes");
            continue;
        }
        exit_confirm = false;

        if (c == 19) { // Ctrl+S
            if (save_file(abs_path, lines, line_count, status)) {
                modified = false;
                saved_once = true;
            }
            continue;
        }

        if (c == KEY_LEFT) {
            if (cur_col > 0) {
                cur_col--;
            } else if (cur_line > 0) {
                cur_line--;
                cur_col = lines[cur_line].len;
            }
            continue;
        }
        if (c == KEY_RIGHT) {
            if (cur_col < lines[cur_line].len) {
                cur_col++;
            } else if (cur_line + 1u < line_count) {
                cur_line++;
                cur_col = 0;
            }
            continue;
        }
        if (c == KEY_UP) {
            if (cur_line > 0) {
                cur_line--;
                clamp_u32(&cur_col, lines[cur_line].len);
            }
            continue;
        }
        if (c == KEY_DOWN) {
            if (cur_line + 1u < line_count) {
                cur_line++;
                clamp_u32(&cur_col, lines[cur_line].len);
            }
            continue;
        }

        if (c == '\n') {
            if (line_count >= EDIT_MAX_LINES) {
                strcpy(status, "too many lines");
                continue;
            }

            edit_line_t* cur = &lines[cur_line];
            edit_line_t* next = &lines[cur_line + 1u];

            // Shift lines down.
            for (uint32_t i = line_count; i > cur_line + 1u; i--) {
                lines[i] = lines[i - 1u];
            }

            memset(next, 0, sizeof(*next));

            uint32_t tail_len = cur->len - cur_col;
            if (tail_len) {
                next->cap = tail_len + 1u;
                next->data = (char*)kmalloc(next->cap);
                if (!next->data) {
                    strcpy(status, "out of memory");
                    // Undo shift best-effort.
                    for (uint32_t i = cur_line + 1u; i < line_count; i++) {
                        lines[i] = lines[i + 1u];
                    }
                    continue;
                }
                memcpy(next->data, cur->data + cur_col, tail_len);
                next->data[tail_len] = '\0';
                next->len = tail_len;
            } else {
                next->data = (char*)kmalloc(1u);
                if (!next->data) {
                    strcpy(status, "out of memory");
                    for (uint32_t i = cur_line + 1u; i < line_count; i++) {
                        lines[i] = lines[i + 1u];
                    }
                    continue;
                }
                next->data[0] = '\0';
                next->len = 0;
                next->cap = 1;
            }

            cur->len = cur_col;
            cur->data[cur->len] = '\0';
            line_count++;
            cur_line++;
            cur_col = 0;
            modified = true;
            continue;
        }

        if (c == '\b') {
            if (cur_col > 0) {
                if (line_delete_char(&lines[cur_line], cur_col - 1u)) {
                    cur_col--;
                    modified = true;
                }
                continue;
            }
            if (cur_line == 0) {
                continue;
            }

            // Merge into previous line.
            edit_line_t* prev = &lines[cur_line - 1u];
            edit_line_t* cur = &lines[cur_line];
            uint32_t prev_len = prev->len;
            if (!line_append(prev, cur)) {
                strcpy(status, "line too long");
                continue;
            }

            if (cur->data) {
                kfree(cur->data);
            }

            // Shift lines up.
            for (uint32_t i = cur_line; i + 1u < line_count; i++) {
                lines[i] = lines[i + 1u];
            }
            memset(&lines[line_count - 1u], 0, sizeof(lines[0]));
            line_count--;

            cur_line--;
            cur_col = prev_len;
            modified = true;
            continue;
        }

        if (c == '\t') {
            for (int i = 0; i < 4; i++) {
                if (line_insert_char(&lines[cur_line], cur_col, ' ')) {
                    cur_col++;
                    modified = true;
                }
            }
            continue;
        }

        if (c >= ' ' && c <= '~') {
            if (line_insert_char(&lines[cur_line], cur_col, c)) {
                cur_col++;
                modified = true;
            } else {
                strcpy(status, "line too long");
            }
            continue;
        }
    }

    // Cleanup.
    for (uint32_t i = 0; i < line_count && i < EDIT_MAX_LINES; i++) {
        if (lines[i].data) {
            kfree(lines[i].data);
            lines[i].data = NULL;
        }
        lines[i].len = 0;
        lines[i].cap = 0;
    }

    return saved_once;
}
