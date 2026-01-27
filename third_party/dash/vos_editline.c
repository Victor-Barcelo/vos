/*
 * VOS libedit compatibility layer using linenoise
 * Implements minimal EditLine/History API for dash shell
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "myhistedit.h"
#include "../../third_party/linenoise/linenoise.h"

/* Maximum history entries */
#define MAX_HISTORY 100
#define MAX_LINE_LEN 4096

/* History structure */
struct vos_history {
    char **entries;
    int count;
    int size;
    int current;  /* Current position for iteration */
};

/* EditLine structure */
struct vos_editline {
    const char *prog;
    FILE *fin;
    FILE *fout;
    FILE *ferr;
    el_pfunc_t prompt_func;
    History *hist;
    char *last_line;  /* Buffer for the last line read */
};

/* Global prompt for linenoise callback */
static el_pfunc_t g_prompt_func = NULL;
static EditLine *g_el = NULL;

/*
 * History implementation
 */

History *history_init(void)
{
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

void history_end(History *h)
{
    if (!h) return;

    for (int i = 0; i < h->count; i++) {
        free(h->entries[i]);
    }
    free(h->entries);
    free(h);
}

int history(History *h, HistEvent *ev, int op, ...)
{
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
        if (newsize > 0 && newsize != h->size) {
            /* Resize not fully implemented - just cap count */
            if (h->count > newsize) {
                for (int i = newsize; i < h->count; i++) {
                    free(h->entries[i]);
                    h->entries[i] = NULL;
                }
                h->count = newsize;
            }
        }
        break;
    }

    case H_ENTER: {
        const char *str = va_arg(ap, const char *);
        if (!str) break;

        /* Add to linenoise history too */
        linenoiseHistoryAdd(str);

        /* Add to our history */
        if (h->count >= h->size) {
            /* Remove oldest */
            free(h->entries[0]);
            memmove(h->entries, h->entries + 1, (h->size - 1) * sizeof(char *));
            h->count--;
        }
        h->entries[h->count] = strdup(str);
        if (h->entries[h->count]) {
            ev->num = h->count + 1;
            ev->str = h->entries[h->count];
            h->count++;
        }
        break;
    }

    case H_APPEND: {
        const char *str = va_arg(ap, const char *);
        if (!str || h->count == 0) break;

        /* Append to last entry */
        int last = h->count - 1;
        size_t oldlen = strlen(h->entries[last]);
        size_t newlen = strlen(str);
        char *newstr = realloc(h->entries[last], oldlen + newlen + 1);
        if (newstr) {
            strcpy(newstr + oldlen, str);
            h->entries[last] = newstr;
            ev->num = last + 1;
            ev->str = h->entries[last];
        }
        break;
    }

    case H_FIRST:
        h->current = h->count - 1;
        if (h->current >= 0) {
            ev->num = h->current + 1;
            ev->str = h->entries[h->current];
            va_end(ap);
            return 0;
        }
        va_end(ap);
        return -1;

    case H_NEXT:
        if (h->current > 0) {
            h->current--;
            ev->num = h->current + 1;
            ev->str = h->entries[h->current];
            va_end(ap);
            return 0;
        }
        va_end(ap);
        return -1;

    case H_PREV:
        if (h->current < h->count - 1) {
            h->current++;
            ev->num = h->current + 1;
            ev->str = h->entries[h->current];
            va_end(ap);
            return 0;
        }
        va_end(ap);
        return -1;

    case H_LAST:
        h->current = 0;
        if (h->count > 0) {
            ev->num = 1;
            ev->str = h->entries[0];
            va_end(ap);
            return 0;
        }
        va_end(ap);
        return -1;

    case H_NEXT_EVENT: {
        int n = va_arg(ap, int);
        if (n > 0 && n <= h->count) {
            h->current = n - 1;
            ev->num = n;
            ev->str = h->entries[n - 1];
            va_end(ap);
            return 0;
        }
        va_end(ap);
        return -1;
    }

    case H_PREV_STR: {
        const char *str = va_arg(ap, const char *);
        if (!str) break;

        /* Search backwards for string */
        for (int i = h->count - 1; i >= 0; i--) {
            if (strstr(h->entries[i], str)) {
                h->current = i;
                ev->num = i + 1;
                ev->str = h->entries[i];
                va_end(ap);
                return 0;
            }
        }
        va_end(ap);
        return -1;
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

EditLine *el_init(const char *prog, FILE *fin, FILE *fout, FILE *ferr)
{
    (void)fin;
    (void)fout;
    (void)ferr;

    EditLine *el = malloc(sizeof(EditLine));
    if (!el) return NULL;

    el->prog = prog;
    el->fin = fin;
    el->fout = fout;
    el->ferr = ferr;
    el->prompt_func = NULL;
    el->hist = NULL;
    el->last_line = NULL;

    g_el = el;

    /* Set up linenoise */
    linenoiseSetMultiLine(1);

    return el;
}

void el_end(EditLine *el)
{
    if (!el) return;

    if (el->last_line) {
        free(el->last_line);
    }

    if (g_el == el) {
        g_el = NULL;
        g_prompt_func = NULL;
    }

    free(el);
}

const char *el_gets(EditLine *el, int *count)
{
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
    char *prompt = "";
    if (el->prompt_func) {
        prompt = el->prompt_func(el);
    }

    /* Read line using linenoise */
    char *line = linenoise(prompt);
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

int el_set(EditLine *el, int op, ...)
{
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
        g_prompt_func = func;
        break;
    }

    case EL_EDITOR: {
        const char *mode = va_arg(ap, const char *);
        (void)mode;  /* linenoise doesn't support vi/emacs mode switching */
        break;
    }

    case EL_HIST: {
        /* int (*func)(History *, HistEvent *, int, ...) */
        va_arg(ap, void *);  /* Skip function pointer */
        History *h = va_arg(ap, History *);
        el->hist = h;
        break;
    }

    case EL_TERMINAL: {
        const char *term = va_arg(ap, const char *);
        (void)term;  /* linenoise handles terminal automatically */
        break;
    }

    default:
        break;
    }

    va_end(ap);
    return 0;
}

int el_source(EditLine *el, const char *file)
{
    (void)el;
    (void)file;
    /* No .editrc support in linenoise */
    return 0;
}
