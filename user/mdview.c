/*
 * mdview - Terminal Markdown Viewer for VOS
 * Uses MD4C library for parsing
 *
 * Usage: mdview [file.md]
 *        cat file.md | mdview
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "../third_party/md4c/md4c.h"

/* ANSI color codes */
#define RESET       "\x1b[0m"
#define BOLD        "\x1b[1m"
#define DIM         "\x1b[2m"
#define ITALIC      "\x1b[3m"
#define UNDERLINE   "\x1b[4m"
#define STRIKETHROUGH "\x1b[9m"

/* Foreground colors */
#define FG_BLACK    "\x1b[30m"
#define FG_RED      "\x1b[31m"
#define FG_GREEN    "\x1b[32m"
#define FG_YELLOW   "\x1b[33m"
#define FG_BLUE     "\x1b[34m"
#define FG_MAGENTA  "\x1b[35m"
#define FG_CYAN     "\x1b[36m"
#define FG_WHITE    "\x1b[37m"

/* Bright foreground colors */
#define FG_BRIGHT_BLACK    "\x1b[90m"
#define FG_BRIGHT_RED      "\x1b[91m"
#define FG_BRIGHT_GREEN    "\x1b[92m"
#define FG_BRIGHT_YELLOW   "\x1b[93m"
#define FG_BRIGHT_BLUE     "\x1b[94m"
#define FG_BRIGHT_MAGENTA  "\x1b[95m"
#define FG_BRIGHT_CYAN     "\x1b[96m"
#define FG_BRIGHT_WHITE    "\x1b[97m"

/* Background colors */
#define BG_BLACK    "\x1b[40m"
#define BG_RED      "\x1b[41m"
#define BG_GREEN    "\x1b[42m"
#define BG_YELLOW   "\x1b[43m"
#define BG_BLUE     "\x1b[44m"
#define BG_MAGENTA  "\x1b[45m"
#define BG_CYAN     "\x1b[46m"
#define BG_WHITE    "\x1b[47m"
#define BG_BRIGHT_BLACK "\x1b[100m"

/* Render state */
typedef struct {
    int term_width;
    int col;                    /* Current column */
    int indent;                 /* Current indentation level */
    int list_depth;             /* Nested list depth */
    int list_item_num[8];       /* Ordered list item numbers per depth */
    int list_is_ordered[8];     /* 1 if ordered, 0 if unordered */
    int in_list_item;           /* Currently in a list item */
    int in_code_block;          /* Inside a code block */
    int in_code_span;           /* Inside inline code */
    int in_blockquote;          /* Inside blockquote */
    int in_table;               /* Inside table */
    int table_col;              /* Current table column */
    int table_cols;             /* Total table columns */
    int is_bold;                /* Bold active */
    int is_italic;              /* Italic active */
    int is_underline;           /* Underline active */
    int is_strikethrough;       /* Strikethrough active */
    int need_newline;           /* Need newline before next block */
    int suppress_newline;       /* Suppress automatic newlines */
    char link_url[512];         /* Current link URL */
    int in_link;                /* Inside a link */
} render_state_t;

static render_state_t state;

/* Get terminal width */
static int get_term_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return 80; /* Default */
}

/* Output a string */
static void out(const char *s) {
    fputs(s, stdout);
}

/* Output n characters from a string */
static void out_n(const char *s, size_t n) {
    fwrite(s, 1, n, stdout);
}

/* Output a newline and reset column */
static void newline(void) {
    putchar('\n');
    state.col = 0;
}

/* Output indentation */
static void do_indent(void) {
    int spaces = state.indent * 2;
    if (state.in_blockquote) {
        out(FG_BRIGHT_BLACK);
        out(" > ");
        out(RESET);
        spaces = (state.indent > 0) ? (state.indent - 1) * 2 : 0;
    }
    for (int i = 0; i < spaces; i++) {
        putchar(' ');
    }
    state.col = spaces + (state.in_blockquote ? 3 : 0);
}

/* Word wrap and output text */
static void output_text(const char *text, size_t size) {
    size_t i = 0;
    int max_col = state.term_width - 2;

    while (i < size) {
        /* Handle newlines in text */
        if (text[i] == '\n') {
            newline();
            if (!state.in_code_block) {
                do_indent();
            }
            i++;
            continue;
        }

        /* In code blocks, output verbatim */
        if (state.in_code_block) {
            putchar(text[i]);
            state.col++;
            i++;
            continue;
        }

        /* Find word boundary */
        size_t word_start = i;
        size_t word_end = i;

        /* Skip leading spaces */
        while (word_end < size && text[word_end] == ' ') {
            word_end++;
        }

        /* Find end of word */
        while (word_end < size && text[word_end] != ' ' && text[word_end] != '\n') {
            word_end++;
        }

        size_t word_len = word_end - word_start;

        /* Check if word fits on current line */
        if (state.col + (int)word_len > max_col && state.col > state.indent * 2) {
            newline();
            do_indent();
            /* Skip leading space after wrap */
            if (text[word_start] == ' ') {
                word_start++;
            }
        }

        /* Output the word */
        for (size_t j = word_start; j < word_end && j < size; j++) {
            putchar(text[j]);
            state.col++;
        }

        i = word_end;
    }
}

/* Apply current text style */
static void apply_style(void) {
    out(RESET);
    if (state.is_bold) out(BOLD);
    if (state.is_italic) out(ITALIC);
    if (state.is_underline) out(UNDERLINE);
    if (state.is_strikethrough) out(STRIKETHROUGH);
    if (state.in_code_span) {
        out(FG_CYAN);
    }
    if (state.in_link) {
        out(FG_BLUE);
        out(UNDERLINE);
    }
}

/* Callbacks for MD4C parser */

static int enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    (void)userdata;

    /* Add spacing between blocks */
    if (state.need_newline && !state.suppress_newline) {
        newline();
    }
    state.need_newline = 0;

    switch (type) {
    case MD_BLOCK_DOC:
        break;

    case MD_BLOCK_QUOTE:
        state.in_blockquote = 1;
        state.indent++;
        break;

    case MD_BLOCK_UL:
        if (state.list_depth < 8) {
            state.list_item_num[state.list_depth] = 0;
            state.list_is_ordered[state.list_depth] = 0;
        }
        state.list_depth++;
        state.indent++;
        break;

    case MD_BLOCK_OL: {
        MD_BLOCK_OL_DETAIL *ol = (MD_BLOCK_OL_DETAIL *)detail;
        if (state.list_depth < 8) {
            state.list_item_num[state.list_depth] = ol->start;
            state.list_is_ordered[state.list_depth] = 1;
        }
        state.list_depth++;
        state.indent++;
        break;
    }

    case MD_BLOCK_LI: {
        MD_BLOCK_LI_DETAIL *li = (MD_BLOCK_LI_DETAIL *)detail;
        do_indent();

        if (state.list_depth > 0 && state.list_depth <= 8) {
            int idx = state.list_depth - 1;
            if (state.list_is_ordered[idx]) {
                /* Ordered list */
                out(FG_YELLOW);
                printf("%d. ", state.list_item_num[idx]++);
                out(RESET);
                state.col += 4;
            } else {
                /* Unordered list */
                out(FG_CYAN);
                if (li->is_task) {
                    if (li->task_mark == 'x' || li->task_mark == 'X') {
                        out("[x] ");
                    } else {
                        out("[ ] ");
                    }
                    state.col += 4;
                } else {
                    /* Unicode bullets based on depth */
                    const char *bullets[] = { "\xe2\x80\xa2 ", "\xe2\x97\xa6 ", "\xe2\x96\xaa ", "\xe2\x96\xab " };
                    out(bullets[idx % 4]);
                    state.col += 2;
                }
                out(RESET);
            }
        }
        state.in_list_item = 1;
        state.suppress_newline = 1;
        break;
    }

    case MD_BLOCK_HR:
        newline();
        out(FG_BRIGHT_BLACK);
        for (int i = 0; i < state.term_width - 4; i++) {
            putchar('-');
        }
        out(RESET);
        newline();
        break;

    case MD_BLOCK_H: {
        MD_BLOCK_H_DETAIL *h = (MD_BLOCK_H_DETAIL *)detail;
        newline();

        /* Color based on header level */
        switch (h->level) {
        case 1:
            out(BOLD FG_BRIGHT_CYAN);
            out("# ");
            break;
        case 2:
            out(BOLD FG_BRIGHT_GREEN);
            out("## ");
            break;
        case 3:
            out(BOLD FG_BRIGHT_YELLOW);
            out("### ");
            break;
        case 4:
            out(BOLD FG_BRIGHT_MAGENTA);
            out("#### ");
            break;
        case 5:
            out(BOLD FG_BRIGHT_BLUE);
            out("##### ");
            break;
        default:
            out(BOLD FG_WHITE);
            out("###### ");
            break;
        }
        state.col = h->level + 2;
        break;
    }

    case MD_BLOCK_CODE: {
        MD_BLOCK_CODE_DETAIL *code = (MD_BLOCK_CODE_DETAIL *)detail;
        newline();

        /* Show a nice header with language if specified */
        out(BG_BRIGHT_BLACK FG_WHITE);
        if (code->lang.size > 0) {
            out(" ");
            out_n(code->lang.text, code->lang.size);
            out(" ");
        } else {
            out(" code ");
        }
        out(RESET);
        newline();

        out(FG_GREEN);
        state.in_code_block = 1;
        state.col = 0;
        break;
    }

    case MD_BLOCK_HTML:
        out(FG_BRIGHT_BLACK);
        break;

    case MD_BLOCK_P:
        if (!state.in_list_item) {
            do_indent();
        }
        break;

    case MD_BLOCK_TABLE:
        state.in_table = 1;
        state.table_col = 0;
        if (detail) {
            MD_BLOCK_TABLE_DETAIL *t = (MD_BLOCK_TABLE_DETAIL *)detail;
            state.table_cols = t->col_count;
        }
        newline();
        break;

    case MD_BLOCK_THEAD:
        out(BOLD);
        break;

    case MD_BLOCK_TBODY:
        break;

    case MD_BLOCK_TR:
        do_indent();
        out(FG_BRIGHT_BLACK "| " RESET);
        state.table_col = 0;
        break;

    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        if (state.table_col > 0) {
            out(FG_BRIGHT_BLACK " | " RESET);
        }
        state.table_col++;
        break;
    }

    return 0;
}

static int leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    (void)detail;
    (void)userdata;

    switch (type) {
    case MD_BLOCK_DOC:
        newline();
        break;

    case MD_BLOCK_QUOTE:
        state.in_blockquote = 0;
        state.indent--;
        newline();
        break;

    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
        state.list_depth--;
        state.indent--;
        if (state.list_depth == 0) {
            state.need_newline = 1;
        }
        break;

    case MD_BLOCK_LI:
        state.in_list_item = 0;
        state.suppress_newline = 0;
        newline();
        break;

    case MD_BLOCK_HR:
        state.need_newline = 1;
        break;

    case MD_BLOCK_H:
        out(RESET);
        newline();
        state.need_newline = 1;
        break;

    case MD_BLOCK_CODE:
        out(RESET);
        newline();
        state.in_code_block = 0;
        state.need_newline = 1;
        break;

    case MD_BLOCK_HTML:
        out(RESET);
        state.need_newline = 1;
        break;

    case MD_BLOCK_P:
        newline();
        state.need_newline = 1;
        break;

    case MD_BLOCK_TABLE:
        state.in_table = 0;
        newline();
        state.need_newline = 1;
        break;

    case MD_BLOCK_THEAD:
        out(RESET);
        /* Draw separator line */
        newline();
        do_indent();
        out(FG_BRIGHT_BLACK "|");
        for (int i = 0; i < state.table_cols; i++) {
            out("---|");
        }
        out(RESET);
        newline();
        break;

    case MD_BLOCK_TBODY:
        break;

    case MD_BLOCK_TR:
        out(FG_BRIGHT_BLACK " |" RESET);
        newline();
        break;

    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        break;
    }

    return 0;
}

static int enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
    (void)userdata;

    switch (type) {
    case MD_SPAN_EM:
        state.is_italic = 1;
        apply_style();
        break;

    case MD_SPAN_STRONG:
        state.is_bold = 1;
        apply_style();
        break;

    case MD_SPAN_A: {
        MD_SPAN_A_DETAIL *a = (MD_SPAN_A_DETAIL *)detail;
        state.in_link = 1;
        if (a->href.size > 0 && a->href.size < sizeof(state.link_url) - 1) {
            memcpy(state.link_url, a->href.text, a->href.size);
            state.link_url[a->href.size] = '\0';
        } else {
            state.link_url[0] = '\0';
        }
        apply_style();
        break;
    }

    case MD_SPAN_IMG: {
        MD_SPAN_IMG_DETAIL *img = (MD_SPAN_IMG_DETAIL *)detail;
        out(FG_MAGENTA "[IMG: ");
        state.col += 6;
        if (img->src.size > 0) {
            out_n(img->src.text, img->src.size);
            state.col += img->src.size;
        }
        break;
    }

    case MD_SPAN_CODE:
        state.in_code_span = 1;
        out(FG_CYAN "`");
        state.col++;
        break;

    case MD_SPAN_DEL:
        state.is_strikethrough = 1;
        apply_style();
        break;

    case MD_SPAN_U:
        state.is_underline = 1;
        apply_style();
        break;

    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
        out(FG_YELLOW "$");
        state.col++;
        break;

    case MD_SPAN_WIKILINK:
        out(FG_CYAN "[[");
        state.col += 2;
        break;
    }

    return 0;
}

static int leave_span(MD_SPANTYPE type, void *detail, void *userdata) {
    (void)detail;
    (void)userdata;

    switch (type) {
    case MD_SPAN_EM:
        state.is_italic = 0;
        apply_style();
        break;

    case MD_SPAN_STRONG:
        state.is_bold = 0;
        apply_style();
        break;

    case MD_SPAN_A:
        out(RESET);
        /* Show URL in parentheses if different from text */
        if (state.link_url[0]) {
            out(FG_BRIGHT_BLACK " (");
            out(state.link_url);
            out(")" RESET);
        }
        state.in_link = 0;
        state.link_url[0] = '\0';
        apply_style();
        break;

    case MD_SPAN_IMG:
        out("]" RESET);
        state.col++;
        break;

    case MD_SPAN_CODE:
        out("`" RESET);
        state.col++;
        state.in_code_span = 0;
        apply_style();
        break;

    case MD_SPAN_DEL:
        state.is_strikethrough = 0;
        apply_style();
        break;

    case MD_SPAN_U:
        state.is_underline = 0;
        apply_style();
        break;

    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
        out("$" RESET);
        state.col++;
        break;

    case MD_SPAN_WIKILINK:
        out("]]" RESET);
        state.col += 2;
        break;
    }

    return 0;
}

static int text_callback(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    (void)userdata;

    switch (type) {
    case MD_TEXT_NORMAL:
        output_text(text, size);
        break;

    case MD_TEXT_NULLCHAR:
        /* Replace NULL with replacement character */
        out("\xef\xbf\xbd"); /* U+FFFD */
        state.col++;
        break;

    case MD_TEXT_BR:
        newline();
        do_indent();
        break;

    case MD_TEXT_SOFTBR:
        /* Soft break - just a space */
        putchar(' ');
        state.col++;
        break;

    case MD_TEXT_ENTITY:
        /* Output entity as-is for now */
        out_n(text, size);
        state.col += size;
        break;

    case MD_TEXT_CODE:
        output_text(text, size);
        break;

    case MD_TEXT_HTML:
        out(FG_BRIGHT_BLACK);
        out_n(text, size);
        out(RESET);
        state.col += size;
        break;

    case MD_TEXT_LATEXMATH:
        out(FG_YELLOW);
        out_n(text, size);
        out(RESET);
        state.col += size;
        break;
    }

    return 0;
}

/* Read entire file into memory */
static char *read_file(const char *filename, size_t *size) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) {
        fclose(f);
        return NULL;
    }

    char *content = malloc(fsize + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(content, 1, fsize, f);
    fclose(f);

    content[n] = '\0';
    *size = n;
    return content;
}

/* Read from stdin */
static char *read_stdin(size_t *size) {
    size_t capacity = 4096;
    size_t len = 0;
    char *buf = malloc(capacity);
    if (!buf) return NULL;

    int c;
    while ((c = getchar()) != EOF) {
        if (len + 1 >= capacity) {
            capacity *= 2;
            char *newbuf = realloc(buf, capacity);
            if (!newbuf) {
                free(buf);
                return NULL;
            }
            buf = newbuf;
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    *size = len;
    return buf;
}

static void print_usage(const char *prog) {
    fprintf(stderr, "mdview - Terminal Markdown Viewer\n");
    fprintf(stderr, "Usage: %s [file.md]\n", prog);
    fprintf(stderr, "       cat file.md | %s\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -h, --help    Show this help\n");
}

int main(int argc, char *argv[]) {
    char *content = NULL;
    size_t size = 0;

    /* Parse arguments */
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        content = read_file(argv[1], &size);
        if (!content) {
            fprintf(stderr, "Error: Cannot read file '%s'\n", argv[1]);
            return 1;
        }
    } else {
        /* Read from stdin if not a tty */
        if (isatty(STDIN_FILENO)) {
            print_usage(argv[0]);
            return 1;
        }
        content = read_stdin(&size);
        if (!content) {
            fprintf(stderr, "Error: Cannot read from stdin\n");
            return 1;
        }
    }

    /* Initialize state */
    memset(&state, 0, sizeof(state));
    state.term_width = get_term_width();

    /* Set up parser */
    MD_PARSER parser = {
        .abi_version = 0,
        .flags = MD_DIALECT_GITHUB,  /* GitHub Flavored Markdown */
        .enter_block = enter_block,
        .leave_block = leave_block,
        .enter_span = enter_span,
        .leave_span = leave_span,
        .text = text_callback,
        .debug_log = NULL,
        .syntax = NULL
    };

    /* Parse and render */
    int result = md_parse(content, (MD_SIZE)size, &parser, NULL);

    free(content);

    if (result != 0) {
        fprintf(stderr, "Error: Markdown parsing failed\n");
        return 1;
    }

    return 0;
}
