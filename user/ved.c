#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "syscall.h"

#define VED_VERSION "0.1"
#define VED_TAB_STOP 4
#define VED_STATUS_MSG_TIMEOUT 5

#define CTRL_KEY(k) ((k)&0x1F)

enum editorKey {
    KEY_DEL = 1000,
    KEY_HOME,
    KEY_END,
    KEY_PGUP,
    KEY_PGDN,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
};

typedef struct erow {
    int size;
    char* chars;
} erow_t;

typedef struct editorConfig {
    int cx, cy;
    int rowoff, coloff;
    int screenrows, screencols;
    int textrows;
    int numrows;
    erow_t* row;
    int dirty;
    char filename[256];
    char statusmsg[128];
    time_t statusmsg_time;
    struct termios orig_termios;
    char last_out[256];
} editorConfig_t;

static editorConfig_t E;

static void editorRefreshScreen(void);

typedef struct abuf {
    char* b;
    int len;
} abuf_t;

#define ABUF_INIT {NULL, 0}

static void abAppend(abuf_t* ab, const char* s, int len) {
    if (!ab || !s || len <= 0) {
        return;
    }
    char* newb = (char*)realloc(ab->b, (size_t)ab->len + (size_t)len);
    if (!newb) {
        return;
    }
    memcpy(newb + ab->len, s, (size_t)len);
    ab->b = newb;
    ab->len += len;
}

static void abFree(abuf_t* ab) {
    if (!ab) {
        return;
    }
    free(ab->b);
    ab->b = NULL;
    ab->len = 0;
}

static void editorSetStatusMessage(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

static void editorDie(const char* what) {
    // Best-effort reset.
    (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
    fputs("\x1b[2J\x1b[H\x1b[0m\x1b[?25h", stdout);
    if (what) {
        perror(what);
    }
    exit(1);
}

static void disableRawMode(void) {
    (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
}

static void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        editorDie("tcgetattr");
    }
    struct termios raw = E.orig_termios;
    cfmakeraw(&raw);
    // Keep output post-processing off; ensure we receive ^C as byte if desired.
    raw.c_lflag &= (tcflag_t)~(ISIG);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        editorDie("tcsetattr");
    }
}

static int editorReadKey(void) {
    char c = 0;
    for (;;) {
        int n = (int)read(STDIN_FILENO, &c, 1);
        if (n == 1) break;
        if (n == 0) continue;
        if (errno == EAGAIN) continue;
        editorDie("read");
    }

    if (c == '\x1b') {
        char seq[8] = {0};
        int n0 = (int)read(STDIN_FILENO, &seq[0], 1);
        if (n0 != 1) return '\x1b';
        int n1 = (int)read(STDIN_FILENO, &seq[1], 1);
        if (n1 != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                int i = 2;
                for (; i < (int)sizeof(seq) - 1; i++) {
                    int n = (int)read(STDIN_FILENO, &seq[i], 1);
                    if (n != 1) break;
                    if (seq[i] == '~') break;
                }
                if (seq[i] != '~') return '\x1b';

                if (seq[1] == '1' && seq[2] == '~') return KEY_HOME;
                if (seq[1] == '3' && seq[2] == '~') return KEY_DEL;
                if (seq[1] == '4' && seq[2] == '~') return KEY_END;
                if (seq[1] == '5' && seq[2] == '~') return KEY_PGUP;
                if (seq[1] == '6' && seq[2] == '~') return KEY_PGDN;
                if (seq[1] == '7' && seq[2] == '~') return KEY_HOME;
                if (seq[1] == '8' && seq[2] == '~') return KEY_END;

                if (seq[1] == '1' && seq[2] == '5') return KEY_F5;
                if (seq[1] == '1' && seq[2] == '7') return KEY_F6;
                if (seq[1] == '1' && seq[2] == '8') return KEY_F7;
                if (seq[1] == '1' && seq[2] == '9') return KEY_F8;
                if (seq[1] == '2' && seq[2] == '0') return KEY_F9;
                if (seq[1] == '2' && seq[2] == '1') return KEY_F10;
                return '\x1b';
            }
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
                default: return '\x1b';
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'P': return KEY_F1;
                case 'Q': return KEY_F2;
                case 'R': return KEY_F3;
                case 'S': return KEY_F4;
                default: return '\x1b';
            }
        }
        return '\x1b';
    }

    return (unsigned char)c;
}

static int getWindowSize(int* rows, int* cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return -1;
    }
    if (ws.ws_col == 0 || ws.ws_row == 0) {
        return -1;
    }
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

static void editorUpdateWindowSize(void) {
    int rows = 0;
    int cols = 0;
    if (getWindowSize(&rows, &cols) == -1) {
        // Conservative fallback.
        rows = 24;
        cols = 80;
    }

    E.screenrows = rows;
    E.screencols = cols;

    // Layout: menu bar (1), message bar (1), status bar (1).
    E.textrows = E.screenrows - 3;
    if (E.textrows < 1) {
        E.textrows = 1;
    }
}

static void editorFreeRows(void) {
    for (int i = 0; i < E.numrows; i++) {
        free(E.row[i].chars);
    }
    free(E.row);
    E.row = NULL;
    E.numrows = 0;
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
}

static void editorInsertRow(int at, const char* s, size_t len) {
    if (at < 0 || at > E.numrows) {
        return;
    }
    erow_t* newrows = (erow_t*)realloc(E.row, sizeof(erow_t) * (size_t)(E.numrows + 1));
    if (!newrows) {
        return;
    }
    E.row = newrows;

    if (at < E.numrows) {
        memmove(&E.row[at + 1], &E.row[at], sizeof(erow_t) * (size_t)(E.numrows - at));
    }

    E.row[at].size = (int)len;
    E.row[at].chars = (char*)malloc(len + 1);
    if (!E.row[at].chars) {
        E.row[at].size = 0;
        E.row[at].chars = NULL;
        return;
    }
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.numrows++;
    E.dirty++;
}

static void editorRowInsertChar(erow_t* row, int at, int c) {
    if (!row) {
        return;
    }
    if (at < 0) at = 0;
    if (at > row->size) at = row->size;

    char* newchars = (char*)realloc(row->chars, (size_t)row->size + 2);
    if (!newchars) {
        return;
    }
    row->chars = newchars;
    memmove(&row->chars[at + 1], &row->chars[at], (size_t)(row->size - at + 1));
    row->size++;
    row->chars[at] = (char)c;
    E.dirty++;
}

static void editorRowDelChar(erow_t* row, int at) {
    if (!row || at < 0 || at >= row->size) {
        return;
    }
    memmove(&row->chars[at], &row->chars[at + 1], (size_t)(row->size - at));
    row->size--;
    E.dirty++;
}

static void editorDelRow(int at) {
    if (at < 0 || at >= E.numrows) {
        return;
    }
    free(E.row[at].chars);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow_t) * (size_t)(E.numrows - at - 1));
    E.numrows--;
    E.dirty++;
}

static void editorInsertChar(int c) {
    if (E.cy == E.numrows) {
        editorInsertRow(E.numrows, "", 0);
    }
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

static void editorInsertNewline(void) {
    if (E.cx == 0) {
        editorInsertRow(E.cy, "", 0);
    } else {
        erow_t* row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], (size_t)(row->size - E.cx));
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        E.dirty++;
    }
    E.cy++;
    E.cx = 0;
}

static void editorDelChar(void) {
    if (E.cy >= E.numrows) {
        return;
    }
    if (E.cx == 0 && E.cy == 0) {
        return;
    }

    erow_t* row = &E.row[E.cy];
    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    } else {
        int prev_len = E.row[E.cy - 1].size;
        erow_t* prev = &E.row[E.cy - 1];
        char* newchars = (char*)realloc(prev->chars, (size_t)prev->size + (size_t)row->size + 1);
        if (!newchars) {
            return;
        }
        prev->chars = newchars;
        memcpy(&prev->chars[prev->size], row->chars, (size_t)row->size + 1);
        prev->size += row->size;
        editorDelRow(E.cy);
        E.cy--;
        E.cx = prev_len;
    }
}

static void editorDelForward(void) {
    if (E.cy >= E.numrows) {
        return;
    }
    erow_t* row = &E.row[E.cy];
    if (E.cx < row->size) {
        editorRowDelChar(row, E.cx);
        return;
    }
    if (E.cx == row->size && E.cy < E.numrows - 1) {
        // Join with next row.
        int next_len = E.row[E.cy + 1].size;
        char* newchars = (char*)realloc(row->chars, (size_t)row->size + (size_t)next_len + 1);
        if (!newchars) {
            return;
        }
        row->chars = newchars;
        memcpy(&row->chars[row->size], E.row[E.cy + 1].chars, (size_t)next_len + 1);
        row->size += next_len;
        editorDelRow(E.cy + 1);
    }
}

static void editorOpen(const char* filename) {
    if (!filename || filename[0] == '\0') {
        return;
    }

    editorFreeRows();

    strncpy(E.filename, filename, sizeof(E.filename) - 1u);
    E.filename[sizeof(E.filename) - 1u] = '\0';

    FILE* f = fopen(filename, "r");
    if (!f) {
        editorSetStatusMessage("New file: %s", filename);
        E.dirty = 0;
        return;
    }

    char* line = NULL;
    size_t cap = 0;
    ssize_t nread;
    while ((nread = getline(&line, &cap, f)) != -1) {
        while (nread > 0 && (line[nread - 1] == '\n' || line[nread - 1] == '\r')) {
            nread--;
        }
        editorInsertRow(E.numrows, line, (size_t)nread);
    }
    free(line);
    fclose(f);
    E.dirty = 0;
    editorSetStatusMessage("Opened %s (%d lines)", E.filename, E.numrows);
}

static char* editorRowsToString(int* out_len) {
    int totlen = 0;
    for (int i = 0; i < E.numrows; i++) {
        totlen += E.row[i].size + 1;
    }
    if (out_len) {
        *out_len = totlen;
    }
    if (totlen == 0) {
        char* empty = (char*)malloc(1);
        if (empty) {
            empty[0] = '\0';
        }
        return empty;
    }
    char* buf = (char*)malloc((size_t)totlen);
    if (!buf) {
        return NULL;
    }
    char* p = buf;
    for (int i = 0; i < E.numrows; i++) {
        memcpy(p, E.row[i].chars, (size_t)E.row[i].size);
        p += E.row[i].size;
        *p++ = '\n';
    }
    return buf;
}

static char* editorPrompt(const char* prompt) {
    if (!prompt) {
        prompt = "";
    }

    size_t cap = 128;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) {
        return NULL;
    }
    buf[0] = '\0';

    for (;;) {
        editorSetStatusMessage("%s%s", prompt, buf);

        // Keep the prompt visible by not timing it out while prompting.
        E.statusmsg_time = time(NULL) + 3600;
        editorRefreshScreen();

        int c = editorReadKey();
        if (c == '\x1b') {
            free(buf);
            editorSetStatusMessage("");
            return NULL;
        } else if (c == '\r' || c == '\n') {
            if (len != 0) {
                editorSetStatusMessage("");
                return buf;
            }
        } else if (c == 127 || c == CTRL_KEY('h')) {
            if (len != 0) {
                buf[--len] = '\0';
            }
        } else if (c >= 32 && c <= 126) {
            if (len + 1 >= cap) {
                cap *= 2;
                char* nbuf = (char*)realloc(buf, cap);
                if (!nbuf) {
                    free(buf);
                    return NULL;
                }
                buf = nbuf;
            }
            buf[len++] = (char)c;
            buf[len] = '\0';
        }
    }
}

static void editorSave(void) {
    if (E.filename[0] == '\0') {
        char* name = editorPrompt("Save as: ");
        if (!name) {
            editorSetStatusMessage("Save aborted");
            return;
        }
        strncpy(E.filename, name, sizeof(E.filename) - 1u);
        E.filename[sizeof(E.filename) - 1u] = '\0';
        free(name);
    }

    int len = 0;
    char* buf = editorRowsToString(&len);
    if (!buf) {
        editorSetStatusMessage("Save failed: out of memory");
        return;
    }

    int fd = open(E.filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        free(buf);
        editorSetStatusMessage("Save failed: %s", strerror(errno));
        return;
    }

    int written = 0;
    while (written < len) {
        int n = (int)write(fd, buf + written, (size_t)(len - written));
        if (n <= 0) {
            break;
        }
        written += n;
    }

    if (written == len) {
        E.dirty = 0;
        editorSetStatusMessage("Saved %s (%d bytes)", E.filename, len);
    } else {
        editorSetStatusMessage("Save failed: %s", strerror(errno));
    }

    close(fd);
    free(buf);
}

static void editorScroll(void) {
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.textrows) {
        E.rowoff = E.cy - E.textrows + 1;
    }
    if (E.cx < E.coloff) {
        E.coloff = E.cx;
    }
    if (E.cx >= E.coloff + E.screencols) {
        E.coloff = E.cx - E.screencols + 1;
    }
}

static void editorDrawMenuBar(abuf_t* ab) {
    const char* name = " VED ";
    const char* menu = " File  Edit  Build  Run  Help ";
    abAppend(ab, "\x1b[7m", 4);
    abAppend(ab, name, (int)strlen(name));
    abAppend(ab, menu, (int)strlen(menu));

    // Pad to end while reverse is active.
    int used = (int)strlen(name) + (int)strlen(menu);
    if (used < E.screencols) {
        for (int i = used; i < E.screencols; i++) {
            abAppend(ab, " ", 1);
        }
    }
    abAppend(ab, "\x1b[0m", 4);
    abAppend(ab, "\r\n", 2);
}

static void editorDrawRows(abuf_t* ab) {
    for (int y = 0; y < E.textrows; y++) {
        int filerow = y + E.rowoff;
        abAppend(ab, "\x1b[K", 3);
        if (filerow >= E.numrows) {
            const char* tilde = "\x1b[36m~\x1b[0m";
            abAppend(ab, tilde, (int)strlen(tilde));
        } else {
            int len = E.row[filerow].size - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            if (len > 0) {
                abAppend(ab, &E.row[filerow].chars[E.coloff], len);
            }
        }
        abAppend(ab, "\r\n", 2);
    }
}

static void editorDrawMessageBar(abuf_t* ab) {
    abAppend(ab, "\x1b[K", 3);

    // Show either the transient status message, or key hints.
    int show_msg = 0;
    if (E.statusmsg[0] != '\0') {
        time_t now = time(NULL);
        if ((int)(now - E.statusmsg_time) < VED_STATUS_MSG_TIMEOUT) {
            show_msg = 1;
        }
    }

    if (show_msg) {
        int len = (int)strlen(E.statusmsg);
        if (len > E.screencols) len = E.screencols;
        abAppend(ab, "\x1b[33;1m", 7);
        abAppend(ab, E.statusmsg, len);
        abAppend(ab, "\x1b[0m", 4);
    } else {
        const char* hints = "F2 Save  F3 Open  F9 Build  Ctrl-R Run  Ctrl-Q Quit  Ctrl-S Save  Ctrl-O Open";
        int len = (int)strlen(hints);
        if (len > E.screencols) len = E.screencols;
        abAppend(ab, "\x1b[36m", 5);
        abAppend(ab, hints, len);
        abAppend(ab, "\x1b[0m", 4);
    }

    abAppend(ab, "\r\n", 2);
}

static void editorDrawStatusBar(abuf_t* ab) {
    abAppend(ab, "\x1b[7m", 4);

    char left[128];
    const char* fname = (E.filename[0] != '\0') ? E.filename : "[No Name]";
    snprintf(left, sizeof(left), " %s%s - %d lines ", fname, E.dirty ? "*" : "", E.numrows);

    char right[64];
    snprintf(right, sizeof(right), " Ln %d, Col %d ", E.cy + 1, E.cx + 1);

    int l_len = (int)strlen(left);
    int r_len = (int)strlen(right);

    if (l_len > E.screencols) l_len = E.screencols;
    abAppend(ab, left, l_len);

    while (l_len < E.screencols - r_len) {
        abAppend(ab, " ", 1);
        l_len++;
    }
    if (r_len < E.screencols) {
        abAppend(ab, right, r_len);
    }

    abAppend(ab, "\x1b[0m", 4);
}

static void editorRefreshScreen(void) {
    editorUpdateWindowSize();
    editorScroll();

    abuf_t ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawMenuBar(&ab);
    editorDrawRows(&ab);
    editorDrawMessageBar(&ab);
    editorDrawStatusBar(&ab);

    int cx = (E.cx - E.coloff) + 1;
    int cy = (E.cy - E.rowoff) + 2; // 1-based; +1 for menu bar
    if (cx < 1) cx = 1;
    if (cy < 2) cy = 2;
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cy, cx);
    abAppend(&ab, buf, (int)strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    (void)write(STDOUT_FILENO, ab.b, (size_t)ab.len);
    abFree(&ab);
}

static void editorMoveCursor(int key) {
    erow_t* row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    switch (key) {
        case KEY_LEFT:
            if (E.cx != 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case KEY_RIGHT:
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row && E.cx == row->size && E.cy < E.numrows - 1) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case KEY_UP:
            if (E.cy != 0) {
                E.cy--;
            }
            break;
        case KEY_DOWN:
            if (E.cy < E.numrows) {
                E.cy++;
            }
            break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
}

static int spawn_exec_try(const char* path, const char* const* argv, uint32_t argc) {
    int pid = sys_spawn(path, argv, argc);
    if (pid < 0) {
        return pid;
    }

    int fg = pid;
    (void)ioctl(0, TIOCSPGRP, &fg);
    int code = sys_wait((uint32_t)pid);
    int none = 0;
    (void)ioctl(0, TIOCSPGRP, &none);
    return code;
}

static int run_external_blocking(const char* title, const char* path, const char* const* argv, uint32_t argc) {
    disableRawMode();
    fputs("\x1b[2J\x1b[H\x1b[0m\x1b[?25h", stdout);

    if (title && title[0] != '\0') {
        printf("=== %s ===\n\n", title);
        fflush(stdout);
    }

    int code = spawn_exec_try(path, argv, argc);
    if (code < 0) {
        errno = -code;
        printf("%s: %s\n", path, strerror(errno));
    } else {
        printf("\n(exit %d)\n", code);
    }

    printf("\nPress Enter to return...");
    fflush(stdout);
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }

    enableRawMode();
    return code;
}

static void editorBuild(void) {
    if (E.filename[0] == '\0') {
        editorSetStatusMessage("Build: no file name (save first)");
        return;
    }

    if (E.dirty) {
        editorSave();
        if (E.dirty) {
            editorSetStatusMessage("Build: save failed");
            return;
        }
    }

    const char* out = "/disk/a.out";
    strncpy(E.last_out, out, sizeof(E.last_out) - 1u);
    E.last_out[sizeof(E.last_out) - 1u] = '\0';

    const char* const argv_usr[] = {"/usr/bin/tcc", E.filename, "-o", out};
    int code = run_external_blocking("TCC build", "/usr/bin/tcc", argv_usr, 4u);
    if (code < 0) {
        const char* const argv_bin[] = {"/bin/tcc", E.filename, "-o", out};
        code = run_external_blocking("TCC build", "/bin/tcc", argv_bin, 4u);
    }

    if (code == 0) {
        editorSetStatusMessage("Build OK: %s", out);
    } else if (code > 0) {
        editorSetStatusMessage("Build failed (exit %d)", code);
    } else {
        editorSetStatusMessage("Build failed");
    }
}

static void editorRun(void) {
    const char* prog = (E.last_out[0] != '\0') ? E.last_out : "/disk/a.out";
    const char* const argv[] = {prog};
    int code = run_external_blocking("Run", prog, argv, 1u);
    if (code >= 0) {
        editorSetStatusMessage("Run exit %d", code);
    } else {
        editorSetStatusMessage("Run failed");
    }
}

static void editorHelp(void) {
    disableRawMode();
    fputs("\x1b[2J\x1b[H\x1b[0m\x1b[?25h", stdout);
    printf("VOS Editor (VED) %s\n\n", VED_VERSION);
    puts("Keys:");
    puts("  Arrow keys     Move cursor");
    puts("  Home/End       Line start/end");
    puts("  PgUp/PgDn      Scroll");
    puts("  Backspace/Del  Delete");
    puts("  Enter          New line");
    puts("  Ctrl-S / F2    Save");
    puts("  Ctrl-O / F3    Open");
    puts("  Ctrl-Q         Quit");
    puts("  Ctrl-R         Run /disk/a.out");
    puts("  F9             Build with tcc -> /disk/a.out");
    puts("");
    puts("Tip: Use /home/<user> for source files.");
    puts("");
    printf("Press Enter to return...");
    fflush(stdout);
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
    enableRawMode();
}

static int editorConfirmQuit(void) {
    static int quit_tries = 0;
    if (!E.dirty) {
        quit_tries = 0;
        return 1;
    }
    if (quit_tries == 0) {
        editorSetStatusMessage("Unsaved changes! Press Ctrl-Q again to quit.");
        quit_tries = 1;
        return 0;
    }
    quit_tries = 0;
    return 1;
}

static void editorProcessKeypress(void) {
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            if (editorConfirmQuit()) {
                fputs("\x1b[2J\x1b[H\x1b[0m\x1b[?25h", stdout);
                exit(0);
            }
            break;
        case CTRL_KEY('s'):
        case KEY_F2:
            editorSave();
            break;
        case CTRL_KEY('o'):
        case KEY_F3: {
            char* name = editorPrompt("Open: ");
            if (name) {
                editorOpen(name);
                free(name);
            } else {
                editorSetStatusMessage("Open aborted");
            }
            break;
        }
        case CTRL_KEY('r'):
            editorRun();
            break;
        case CTRL_KEY('b'):
        case KEY_F9:
            editorBuild();
            break;
        case KEY_F1:
            editorHelp();
            break;
        case '\r':
        case '\n':
            editorInsertNewline();
            break;
        case 127:
        case CTRL_KEY('h'):
            editorDelChar();
            break;
        case KEY_DEL:
            editorDelForward();
            break;
        case KEY_HOME:
            E.cx = 0;
            break;
        case KEY_END:
            if (E.cy < E.numrows) {
                E.cx = E.row[E.cy].size;
            }
            break;
        case KEY_PGUP:
        case KEY_PGDN: {
            int times = E.textrows;
            while (times--) {
                editorMoveCursor(c == KEY_PGUP ? KEY_UP : KEY_DOWN);
            }
            break;
        }
        case KEY_UP:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_RIGHT:
            editorMoveCursor(c);
            break;
        case '\t': {
            for (int i = 0; i < VED_TAB_STOP; i++) {
                editorInsertChar(' ');
            }
            break;
        }
        default:
            if (c >= 32 && c <= 126) {
                editorInsertChar(c);
            }
            break;
    }
}

static void editorInit(void) {
    memset(&E, 0, sizeof(E));
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename[0] = '\0';
    E.statusmsg[0] = '\0';
    E.last_out[0] = '\0';

    editorUpdateWindowSize();
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    editorInit();
    enableRawMode();

    fputs("\x1b[2J\x1b[H\x1b[?25l", stdout);

    if (argc >= 2 && argv[1]) {
        editorOpen(argv[1]);
    } else {
        editorSetStatusMessage("VED %s - Ctrl-O open, Ctrl-S save, Ctrl-Q quit, F1 help", VED_VERSION);
    }

    for (;;) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
}
