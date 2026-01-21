#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jsmn.h>

static void print_usage(void) {
    puts("Usage: json <file>");
    puts("Pretty-print a JSON file (jsmn-based).");
}

static char* read_all(const char* path, size_t* out_len) {
    if (out_len) *out_len = 0;
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        errno = EIO;
        return NULL;
    }
    long end = ftell(fp);
    if (end < 0) {
        fclose(fp);
        errno = EIO;
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        errno = EIO;
        return NULL;
    }

    size_t len = (size_t)end;
    char* buf = (char*)malloc(len + 1u);
    if (!buf) {
        fclose(fp);
        errno = ENOMEM;
        return NULL;
    }

    size_t got = fread(buf, 1, len, fp);
    fclose(fp);
    if (got != len) {
        free(buf);
        errno = EIO;
        return NULL;
    }

    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

static void indent(int n) {
    for (int i = 0; i < n; i++) {
        putchar(' ');
    }
}

static void print_json_slice_quoted(const char* json, const jsmntok_t* t) {
    putchar('"');
    fwrite(json + t->start, 1, (size_t)(t->end - t->start), stdout);
    putchar('"');
}

static int print_value(const char* json, const jsmntok_t* toks, int i, int ind) {
    const jsmntok_t* t = &toks[i];
    switch (t->type) {
        case JSMN_OBJECT: {
            int pairs = t->size;
            puts("{");
            int j = i + 1;
            for (int p = 0; p < pairs; p++) {
                indent(ind + 2);
                const jsmntok_t* key = &toks[j++];
                print_json_slice_quoted(json, key);
                fputs(": ", stdout);
                j = print_value(json, toks, j, ind + 2);
                if (p != pairs - 1) putchar(',');
                putchar('\n');
            }
            indent(ind);
            putchar('}');
            return j;
        }
        case JSMN_ARRAY: {
            int count = t->size;
            puts("[");
            int j = i + 1;
            for (int k = 0; k < count; k++) {
                indent(ind + 2);
                j = print_value(json, toks, j, ind + 2);
                if (k != count - 1) putchar(',');
                putchar('\n');
            }
            indent(ind);
            putchar(']');
            return j;
        }
        case JSMN_STRING:
            print_json_slice_quoted(json, t);
            return i + 1;
        case JSMN_PRIMITIVE:
            fwrite(json + t->start, 1, (size_t)(t->end - t->start), stdout);
            return i + 1;
        default:
            fputs("null", stdout);
            return i + 1;
    }
}

int main(int argc, char** argv) {
    if (argc != 2 || strcmp(argv[1], "--help") == 0) {
        print_usage();
        return (argc == 2 && strcmp(argv[1], "--help") == 0) ? 0 : 1;
    }

    const char* path = argv[1];
    size_t len = 0;
    char* json = read_all(path, &len);
    if (!json) {
        fprintf(stderr, "json: failed to read %s: %s\n", path, strerror(errno));
        return 1;
    }

    jsmn_parser p;
    jsmn_init(&p);

    int tok_cap = 256;
    jsmntok_t* toks = NULL;

    for (;;) {
        toks = (jsmntok_t*)realloc(toks, (size_t)tok_cap * sizeof(*toks));
        if (!toks) {
            fprintf(stderr, "json: out of memory\n");
            free(json);
            return 1;
        }

        jsmn_init(&p);
        int rc = jsmn_parse(&p, json, (int)len, toks, tok_cap);
        if (rc == JSMN_ERROR_NOMEM) {
            tok_cap *= 2;
            if (tok_cap > 8192) {
                fprintf(stderr, "json: document too complex\n");
                free(toks);
                free(json);
                return 1;
            }
            continue;
        }
        if (rc < 0) {
            fprintf(stderr, "json: parse error (%d)\n", rc);
            free(toks);
            free(json);
            return 1;
        }
        break;
    }

    if (tok_cap == 0) {
        free(toks);
        free(json);
        return 0;
    }

    (void)print_value(json, toks, 0, 0);
    putchar('\n');

    free(toks);
    free(json);
    return 0;
}

