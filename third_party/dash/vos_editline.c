/*
 * VOS libedit compatibility layer - simple implementation
 * Uses basic stdio for input (no fancy line editing for now)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "myhistedit.h"

/* Maximum history entries */
#define MAX_HISTORY 100
#define MAX_LINE_LEN 4096

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
};

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
        (void)newsize;
        break;
    }

    case H_ENTER: {
        const char *str = va_arg(ap, const char *);
        if (!str || !str[0]) break;

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

    default:
        break;
    }

    va_end(ap);
    return 0;
}

/*
 * EditLine implementation - simple version using fgets
 */

EditLine *el_init(const char *prog, FILE *fin, FILE *fout, FILE *ferr)
{
    EditLine *el = malloc(sizeof(EditLine));
    if (!el) return NULL;

    el->prog = prog;
    el->fin = fin ? fin : stdin;
    el->fout = fout ? fout : stdout;
    el->ferr = ferr ? ferr : stderr;
    el->prompt_func = NULL;
    el->hist = NULL;
    el->last_line = NULL;

    return el;
}

void el_end(EditLine *el)
{
    if (!el) return;

    if (el->last_line) {
        free(el->last_line);
    }
    free(el);
}

const char *el_gets(EditLine *el, int *count)
{
    static char buf[MAX_LINE_LEN];

    if (!el) {
        if (count) *count = 0;
        return NULL;
    }

    /* Free previous line */
    if (el->last_line) {
        free(el->last_line);
        el->last_line = NULL;
    }

    /* Print prompt */
    if (el->prompt_func) {
        char *prompt = el->prompt_func(el);
        if (prompt) {
            fputs(prompt, el->fout);
            fflush(el->fout);
        }
    }

    /* Read line using fgets */
    if (!fgets(buf, sizeof(buf), el->fin)) {
        if (count) *count = 0;
        return NULL;
    }

    /* Return the line (fgets already includes newline) */
    size_t len = strlen(buf);
    el->last_line = strdup(buf);
    if (!el->last_line) {
        if (count) *count = 0;
        return NULL;
    }

    if (count) *count = (int)len;
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

int el_source(EditLine *el, const char *file)
{
    (void)el;
    (void)file;
    return 0;
}
