/*
 * VOS libedit compatibility layer with line editing and history
 * Simple implementation that works with VOS's terminal handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include "myhistedit.h"

#define MAX_HISTORY 100
#define MAX_LINE_LEN 1024
#define HISTORY_FILE ".dash_history"

/* History structure */
struct vos_history {
    char **entries;
    int count;
    int size;
    int current;
};

/* EditLine structure */
struct vos_editline {
    const char *prog;
    FILE *fin;
    FILE *fout;
    FILE *ferr;
    el_pfunc_t prompt_func;
    History *hist;
    char *last_line;
    struct termios orig_termios;
    int raw_mode;
};

static EditLine *g_el = NULL;

/* Enable raw mode for line editing */
static int enable_raw_mode(EditLine *el) {
    struct termios raw;

    if (el->raw_mode) return 0;

    if (tcgetattr(STDIN_FILENO, &el->orig_termios) < 0) {
        return -1;
    }

    raw = el->orig_termios;
    /* Input: no break, no CR->NL, no parity, no strip, no flow control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* Output: keep post-processing for proper newline handling */
    /* raw.c_oflag &= ~(OPOST); */
    /* Control: 8-bit chars */
    raw.c_cflag |= CS8;
    /* Local: no echo, no canonical, no signals */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* Return after 1 byte, no timeout */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        return -1;
    }

    el->raw_mode = 1;
    return 0;
}

/* Disable raw mode */
static void disable_raw_mode(EditLine *el) {
    if (el->raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &el->orig_termios);
        el->raw_mode = 0;
    }
}

/* Write string to output */
static void el_write(EditLine *el, const char *s) {
    fputs(s, el->fout);
    fflush(el->fout);
}

/* Calculate visible width of string, skipping ANSI escape sequences */
static int visible_strlen(const char *s) {
    if (!s) return 0;
    int len = 0;
    int in_escape = 0;
    while (*s) {
        if (in_escape) {
            /* End of escape sequence at letter or ~ */
            if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '~') {
                in_escape = 0;
            }
        } else if (*s == '\033') {
            in_escape = 1;
        } else if ((unsigned char)*s >= 32) {
            len++;
        }
        s++;
    }
    return len;
}

/* Forward declaration */
static void refresh_line(EditLine *el, const char *prompt, char *buf, int len, int pos);

/* Tab completion helper - find matches in directory */
static int find_matches(const char *dir, const char *prefix, char **matches, int max_matches) {
    DIR *d = opendir(dir);
    if (!d) return 0;

    int count = 0;
    size_t prefix_len = strlen(prefix);
    struct dirent *ent;

    while ((ent = readdir(d)) != NULL && count < max_matches) {
        /* Skip . and .. */
        if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' ||
            (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
            continue;
        }
        /* Check if prefix matches */
        if (prefix_len == 0 || strncmp(ent->d_name, prefix, prefix_len) == 0) {
            matches[count] = strdup(ent->d_name);
            if (matches[count]) count++;
        }
    }
    closedir(d);
    return count;
}

/* Find common prefix among matches */
static int common_prefix_len(char **matches, int count) {
    if (count <= 0 || !matches[0]) return 0;
    if (count == 1) return strlen(matches[0]);

    int len = 0;
    while (1) {
        char c = matches[0][len];
        if (c == '\0') break;
        int all_match = 1;
        for (int i = 1; i < count; i++) {
            if (matches[i][len] != c) {
                all_match = 0;
                break;
            }
        }
        if (!all_match) break;
        len++;
    }
    return len;
}

/* Free matches array */
static void free_matches(char **matches, int count) {
    for (int i = 0; i < count; i++) {
        free(matches[i]);
    }
}

/* Tab completion - returns number of characters added */
static int do_tab_complete(EditLine *el, char *buf, int *len, int *pos, const char *prompt) {
    #define MAX_MATCHES 64

    /* Find start of current word (stop only at spaces, not slashes) */
    int word_start = *pos;
    while (word_start > 0 && buf[word_start - 1] != ' ') {
        word_start--;
    }

    /* Check if this is first word (command completion) */
    int is_command = 1;
    for (int i = 0; i < word_start; i++) {
        if (buf[i] != ' ') {
            is_command = 0;
            break;
        }
    }

    /* Extract the prefix to complete */
    char prefix[256];
    int prefix_len = *pos - word_start;
    if (prefix_len >= (int)sizeof(prefix)) prefix_len = sizeof(prefix) - 1;
    strncpy(prefix, buf + word_start, prefix_len);
    prefix[prefix_len] = '\0';

    /* Find directory to search */
    char dir[256] = ".";
    char *file_prefix = prefix;

    /* Check if there's a path component */
    char *last_slash = strrchr(prefix, '/');
    if (last_slash) {
        int dir_len = last_slash - prefix;
        if (dir_len == 0) {
            strcpy(dir, "/");
        } else {
            strncpy(dir, prefix, dir_len);
            dir[dir_len] = '\0';
        }
        file_prefix = last_slash + 1;
        word_start = *pos - strlen(file_prefix);
    } else if (is_command) {
        strcpy(dir, "/bin");
    }

    /* Find matches */
    char *matches[MAX_MATCHES];
    int match_count = find_matches(dir, file_prefix, matches, MAX_MATCHES);

    if (match_count == 0) {
        /* No matches - beep */
        el_write(el, "\a");
        return 0;
    }

    int added = 0;
    int common_len = common_prefix_len(matches, match_count);
    int file_prefix_len = strlen(file_prefix);

    if (common_len > file_prefix_len) {
        /* Complete the common prefix */
        int to_add = common_len - file_prefix_len;

        /* Make room in buffer */
        if (*len + to_add < MAX_LINE_LEN) {
            memmove(buf + *pos + to_add, buf + *pos, *len - *pos + 1);
            memcpy(buf + *pos, matches[0] + file_prefix_len, to_add);
            *len += to_add;
            *pos += to_add;
            added = to_add;
            buf[*len] = '\0';
        }

        /* If single match, check if it's a directory and add / or space */
        if (match_count == 1) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, matches[0]);
            struct stat st;
            if (stat(full_path, &st) == 0) {
                char suffix = S_ISDIR(st.st_mode) ? '/' : ' ';
                if (*len < MAX_LINE_LEN - 1) {
                    memmove(buf + *pos + 1, buf + *pos, *len - *pos + 1);
                    buf[*pos] = suffix;
                    (*len)++;
                    (*pos)++;
                    added++;
                    buf[*len] = '\0';
                }
            }
        }
    }

    if (match_count > 1 && added == 0) {
        /* Multiple matches and nothing to complete - show them */
        el_write(el, "\n");
        for (int i = 0; i < match_count; i++) {
            /* Check if directory */
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, matches[i]);
            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                fprintf(el->fout, "%s/  ", matches[i]);
            } else {
                fprintf(el->fout, "%s  ", matches[i]);
            }
        }
        el_write(el, "\n");
        /* Redraw prompt and line */
        refresh_line(el, prompt, buf, *len, *pos);
    }

    free_matches(matches, match_count);
    return added;
}

/* Refresh the line display */
static void refresh_line(EditLine *el, const char *prompt, char *buf, int len, int pos) {
    char seq[64];

    /* Move cursor to start of line */
    el_write(el, "\r");

    /* Write prompt */
    if (prompt) el_write(el, prompt);

    /* Write buffer */
    fwrite(buf, 1, len, el->fout);

    /* Erase to end of line */
    el_write(el, "\x1b[K");

    /* Move cursor to correct position - use visible width for ANSI prompts */
    int prompt_len = visible_strlen(prompt);
    snprintf(seq, sizeof(seq), "\r\x1b[%dC", prompt_len + pos);
    el_write(el, seq);

    fflush(el->fout);
}

/* Line editing with history support */
static char *line_edit(EditLine *el, const char *prompt) {
    static char buf[MAX_LINE_LEN];
    int len = 0;
    int pos = 0;
    int hist_idx = -1;
    char saved_line[MAX_LINE_LEN] = "";

    /* Clear the entire buffer to prevent stale data issues */
    memset(buf, 0, sizeof(buf));

    /* Print prompt */
    if (prompt) el_write(el, prompt);

    /* Enable raw mode */
    if (enable_raw_mode(el) < 0) {
        /* Fallback to fgets if raw mode fails */
        disable_raw_mode(el);
        if (!fgets(buf, sizeof(buf), stdin)) {
            return NULL;
        }
        return strdup(buf);
    }

    /* Input buffer for handling escape sequences - reset each call */
    unsigned char inbuf[16];
    int inbuf_len = 0;
    int inbuf_pos = 0;

    /* Track if we're in the middle of an escape sequence */
    int in_escape = 0;  /* 0=none, 1=got ESC, 2=got ESC[ */

    while (1) {
        unsigned char c;

        /* Refill input buffer if empty */
        if (inbuf_pos >= inbuf_len) {
            inbuf_pos = 0;
            inbuf_len = read(STDIN_FILENO, inbuf, sizeof(inbuf));
            if (inbuf_len <= 0) {
                disable_raw_mode(el);
                inbuf_len = 0;
                return NULL;
            }
        }

        c = inbuf[inbuf_pos++];

        /* State machine for escape sequences */
        if (in_escape == 1) {
            /* We got ESC, expecting '[' */
            if (c == '[') {
                in_escape = 2;
                continue;
            } else {
                /* Not a CSI sequence, ignore the ESC and reprocess this char */
                in_escape = 0;
                /* Fall through to regular character handling */
            }
        } else if (in_escape == 2) {
            /* We got ESC[, expecting command character */
            in_escape = 0;  /* Reset state */
            switch (c) {
                case 'A':  /* Up arrow - previous history */
                    if (el->hist && el->hist->count > 0) {
                        /* Save current line first time */
                        if (hist_idx == -1) {
                            strncpy(saved_line, buf, sizeof(saved_line)-1);
                            saved_line[sizeof(saved_line)-1] = '\0';
                        }
                        if (hist_idx < el->hist->count - 1) {
                            hist_idx++;
                            int idx = el->hist->count - 1 - hist_idx;
                            strncpy(buf, el->hist->entries[idx], sizeof(buf)-1);
                            buf[sizeof(buf)-1] = '\0';
                            len = pos = strlen(buf);
                            refresh_line(el, prompt, buf, len, pos);
                        }
                    }
                    break;
                case 'B':  /* Down arrow - next history */
                    if (hist_idx > 0) {
                        hist_idx--;
                        int idx = el->hist->count - 1 - hist_idx;
                        strncpy(buf, el->hist->entries[idx], sizeof(buf)-1);
                        buf[sizeof(buf)-1] = '\0';
                        len = pos = strlen(buf);
                        refresh_line(el, prompt, buf, len, pos);
                    } else if (hist_idx == 0) {
                        hist_idx = -1;
                        strncpy(buf, saved_line, sizeof(buf)-1);
                        buf[sizeof(buf)-1] = '\0';
                        len = pos = strlen(buf);
                        refresh_line(el, prompt, buf, len, pos);
                    }
                    break;
                case 'C':  /* Right arrow */
                    if (pos < len) {
                        pos++;
                        el_write(el, "\x1b[C");
                    }
                    break;
                case 'D':  /* Left arrow */
                    if (pos > 0) {
                        pos--;
                        el_write(el, "\x1b[D");
                    }
                    break;
                case 'H':  /* Home */
                    pos = 0;
                    refresh_line(el, prompt, buf, len, pos);
                    break;
                case 'F':  /* End */
                    pos = len;
                    refresh_line(el, prompt, buf, len, pos);
                    break;
                case '3':  /* Delete key (ESC [ 3 ~) */
                    if (inbuf_pos < inbuf_len && inbuf[inbuf_pos] == '~') {
                        inbuf_pos++;
                        if (pos < len) {
                            memmove(buf + pos, buf + pos + 1, len - pos);
                            len--;
                            buf[len] = '\0';
                            refresh_line(el, prompt, buf, len, pos);
                        }
                    }
                    break;
            }
            continue;
        }

        /* Handle ESC - start of escape sequence */
        if (c == 27) {
            in_escape = 1;
            continue;
        }

        /* Enter */
        if (c == '\r' || c == '\n') {
            el_write(el, "\n");
            disable_raw_mode(el);
            buf[len] = '\0';
            return strdup(buf);
        }

        /* Ctrl-C */
        if (c == 3) {
            el_write(el, "^C\n");
            disable_raw_mode(el);
            errno = EAGAIN;
            return NULL;
        }

        /* Ctrl-D (EOF) */
        if (c == 4) {
            if (len == 0) {
                disable_raw_mode(el);
                return NULL;
            }
            /* Delete char at cursor */
            if (pos < len) {
                memmove(buf + pos, buf + pos + 1, len - pos);
                len--;
                buf[len] = '\0';
                refresh_line(el, prompt, buf, len, pos);
            }
            continue;
        }

        /* Backspace */
        if (c == 127 || c == 8) {
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - pos + 1);
                pos--;
                len--;
                refresh_line(el, prompt, buf, len, pos);
            }
            continue;
        }

        /* Ctrl-A (home) */
        if (c == 1) {
            pos = 0;
            refresh_line(el, prompt, buf, len, pos);
            continue;
        }

        /* Ctrl-E (end) */
        if (c == 5) {
            pos = len;
            refresh_line(el, prompt, buf, len, pos);
            continue;
        }

        /* Ctrl-K (kill to end) */
        if (c == 11) {
            buf[pos] = '\0';
            len = pos;
            refresh_line(el, prompt, buf, len, pos);
            continue;
        }

        /* Ctrl-U (kill line) */
        if (c == 21) {
            buf[0] = '\0';
            len = pos = 0;
            refresh_line(el, prompt, buf, len, pos);
            continue;
        }

        /* Ctrl-L (clear screen) */
        if (c == 12) {
            el_write(el, "\x1b[H\x1b[2J");
            refresh_line(el, prompt, buf, len, pos);
            continue;
        }

        /* Tab (completion) */
        if (c == '\t') {
            do_tab_complete(el, buf, &len, &pos, prompt);
            refresh_line(el, prompt, buf, len, pos);
            continue;
        }

        /* Regular character */
        if (c >= 32 && len < MAX_LINE_LEN - 1) {
            if (pos == len) {
                buf[len++] = c;
                buf[len] = '\0';
                pos++;
                char s[2] = {c, 0};
                el_write(el, s);
            } else {
                memmove(buf + pos + 1, buf + pos, len - pos + 1);
                buf[pos] = c;
                len++;
                pos++;
                refresh_line(el, prompt, buf, len, pos);
            }
        }
    }
}

/*
 * History implementation
 */

History *history_init(void) {
    History *h = malloc(sizeof(History));
    if (!h) return NULL;

    h->entries = calloc(MAX_HISTORY, sizeof(char *));
    if (!h->entries) {
        free(h);
        return NULL;
    }

    h->count = 0;
    h->size = MAX_HISTORY;
    h->current = -1;

    return h;
}

void history_end(History *h) {
    if (!h) return;

    for (int i = 0; i < h->count; i++) {
        free(h->entries[i]);
    }
    free(h->entries);
    free(h);
}

/* Load history from file */
static void history_load(History *h, const char *home) {
    if (!h || !home) return;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", home, HISTORY_FILE);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        /* Remove trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        if (len > 0 && h->count < h->size) {
            h->entries[h->count++] = strdup(line);
        }
    }
    fclose(f);
}

/* Save history to file */
static void history_save(History *h, const char *home) {
    if (!h || !home || h->count == 0) return;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", home, HISTORY_FILE);

    FILE *f = fopen(path, "w");
    if (!f) return;

    for (int i = 0; i < h->count; i++) {
        fprintf(f, "%s\n", h->entries[i]);
    }
    fclose(f);
}

int history(History *h, HistEvent *ev, int op, ...) {
    va_list ap;
    va_start(ap, op);

    if (!h || !ev) {
        va_end(ap);
        return -1;
    }

    ev->num = 0;
    ev->str = NULL;

    switch (op) {
    case H_SETSIZE: {
        int newsize = va_arg(ap, int);
        (void)newsize;
        break;
    }

    case H_ENTER: {
        const char *str = va_arg(ap, const char *);
        if (!str || !str[0]) break;

        /* Strip trailing newline */
        char *clean = strdup(str);
        if (clean) {
            size_t len = strlen(clean);
            while (len > 0 && (clean[len-1] == '\n' || clean[len-1] == '\r')) {
                clean[--len] = '\0';
            }

            /* Don't add duplicates */
            if (h->count > 0 && strcmp(h->entries[h->count-1], clean) == 0) {
                free(clean);
                break;
            }

            /* Add to history */
            if (h->count >= h->size) {
                free(h->entries[0]);
                memmove(h->entries, h->entries + 1, (h->size - 1) * sizeof(char *));
                h->count--;
            }
            h->entries[h->count] = clean;
            ev->num = h->count + 1;
            ev->str = h->entries[h->count];
            h->count++;

            /* Auto-save history */
            const char *home = getenv("HOME");
            if (home) {
                history_save(h, home);
            }
        }
        break;
    }

    case H_LOAD: {
        const char *file = va_arg(ap, const char *);
        if (file) {
            history_load(h, file);
        }
        break;
    }

    case H_SAVE: {
        const char *file = va_arg(ap, const char *);
        if (file) {
            history_save(h, file);
        }
        break;
    }

    default:
        break;
    }

    va_end(ap);
    return 0;
}

/*
 * EditLine implementation
 */

EditLine *el_init(const char *prog, FILE *fin, FILE *fout, FILE *ferr) {
    EditLine *el = malloc(sizeof(EditLine));
    if (!el) return NULL;

    el->prog = prog;
    el->fin = fin ? fin : stdin;
    el->fout = fout ? fout : stdout;
    el->ferr = ferr ? ferr : stderr;
    el->prompt_func = NULL;
    el->hist = NULL;
    el->last_line = NULL;
    el->raw_mode = 0;

    g_el = el;

    return el;
}

void el_end(EditLine *el) {
    if (!el) return;

    disable_raw_mode(el);

    if (el->last_line) {
        free(el->last_line);
    }

    if (g_el == el) {
        g_el = NULL;
    }

    free(el);
}

const char *el_gets(EditLine *el, int *count) {
    if (!el) {
        if (count) *count = 0;
        return NULL;
    }

    /* Free previous line */
    if (el->last_line) {
        free(el->last_line);
        el->last_line = NULL;
    }

    /* Get prompt */
    char *prompt = NULL;
    if (el->prompt_func) {
        prompt = el->prompt_func(el);
    }

    /* Read line with editing */
    char *line = line_edit(el, prompt);
    if (!line) {
        if (count) *count = 0;
        return NULL;
    }

    /* Add newline (dash expects it) */
    size_t len = strlen(line);
    el->last_line = malloc(len + 2);
    if (!el->last_line) {
        free(line);
        if (count) *count = 0;
        return NULL;
    }

    memcpy(el->last_line, line, len);
    el->last_line[len] = '\n';
    el->last_line[len + 1] = '\0';

    free(line);

    if (count) *count = len + 1;
    return el->last_line;
}

int el_set(EditLine *el, int op, ...) {
    va_list ap;
    va_start(ap, op);

    if (!el) {
        va_end(ap);
        return -1;
    }

    switch (op) {
    case EL_PROMPT: {
        el_pfunc_t func = va_arg(ap, el_pfunc_t);
        el->prompt_func = func;
        break;
    }

    case EL_EDITOR: {
        const char *mode = va_arg(ap, const char *);
        (void)mode;
        break;
    }

    case EL_HIST: {
        va_arg(ap, void *);  /* Skip function pointer */
        History *h = va_arg(ap, History *);
        el->hist = h;
        /* Load history from file */
        const char *home = getenv("HOME");
        if (home && h) {
            history_load(h, home);
        }
        break;
    }

    case EL_TERMINAL: {
        const char *term = va_arg(ap, const char *);
        (void)term;
        break;
    }

    default:
        break;
    }

    va_end(ap);
    return 0;
}

int el_source(EditLine *el, const char *file) {
    (void)el;
    (void)file;
    return 0;
}
