#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct tree_entry {
    char* name;
    bool is_dir;
} tree_entry_t;

static int entry_cmp(const void* a, const void* b) {
    const tree_entry_t* ea = (const tree_entry_t*)a;
    const tree_entry_t* eb = (const tree_entry_t*)b;
    if (!ea->name && !eb->name) return 0;
    if (!ea->name) return -1;
    if (!eb->name) return 1;
    return strcmp(ea->name, eb->name);
}

static void free_entries(tree_entry_t* ents, size_t n) {
    if (!ents) return;
    for (size_t i = 0; i < n; i++) {
        free(ents[i].name);
    }
    free(ents);
}

static int read_dir(const char* path, bool show_all, tree_entry_t** out_ents, size_t* out_n) {
    *out_ents = NULL;
    *out_n = 0;

    DIR* d = opendir(path);
    if (!d) {
        return -1;
    }

    tree_entry_t* ents = NULL;
    size_t count = 0;
    size_t cap = 0;

    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        const char* name = de->d_name;
        if (!name || name[0] == '\0') continue;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (!show_all && name[0] == '.') continue;

        if (count == cap) {
            size_t new_cap = cap ? cap * 2u : 32u;
            tree_entry_t* next = (tree_entry_t*)realloc(ents, new_cap * sizeof(*ents));
            if (!next) {
                closedir(d);
                free_entries(ents, count);
                errno = ENOMEM;
                return -1;
            }
            ents = next;
            cap = new_cap;
        }

        char full[512];
        if (strcmp(path, "/") == 0) {
            snprintf(full, sizeof(full), "/%s", name);
        } else {
            snprintf(full, sizeof(full), "%s/%s", path, name);
        }

        struct stat st;
        bool is_dir = false;
        if (lstat(full, &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
        }

        ents[count].name = strdup(name);
        ents[count].is_dir = is_dir;
        if (!ents[count].name) {
            closedir(d);
            free_entries(ents, count);
            errno = ENOMEM;
            return -1;
        }
        count++;
    }

    closedir(d);
    qsort(ents, count, sizeof(*ents), entry_cmp);
    *out_ents = ents;
    *out_n = count;
    return 0;
}

static void print_tree(const char* path, const char* prefix, bool show_all, int depth, int max_depth,
                       unsigned long* out_dirs, unsigned long* out_files) {
    tree_entry_t* ents = NULL;
    size_t n = 0;

    if (read_dir(path, show_all, &ents, &n) != 0) {
        printf("%s[error: %s]\n", prefix, strerror(errno));
        return;
    }

    for (size_t i = 0; i < n; i++) {
        const bool last = (i + 1 == n);
        const char* branch = last ? "`-- " : "|-- ";

        printf("%s%s%s%s\n", prefix, branch, ents[i].name, ents[i].is_dir ? "/" : "");

        if (ents[i].is_dir) {
            if (out_dirs) (*out_dirs)++;
        } else {
            if (out_files) (*out_files)++;
        }

        if (ents[i].is_dir && (max_depth < 0 || depth < max_depth)) {
            char child[512];
            if (strcmp(path, "/") == 0) {
                snprintf(child, sizeof(child), "/%s", ents[i].name);
            } else {
                snprintf(child, sizeof(child), "%s/%s", path, ents[i].name);
            }

            char next_prefix[512];
            snprintf(next_prefix, sizeof(next_prefix), "%s%s", prefix, last ? "    " : "|   ");
            print_tree(child, next_prefix, show_all, depth + 1, max_depth, out_dirs, out_files);
        }
    }

    free_entries(ents, n);
}

static void usage(void) {
    puts("Usage: tree [options] [path]");
    puts("Options:");
    puts("  -a        show hidden files");
    puts("  -L <n>    max display depth");
}

int main(int argc, char** argv) {
    const char* path = ".";
    bool show_all = false;
    int max_depth = -1; // unlimited

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (!a) continue;
        if (strcmp(a, "--help") == 0) {
            usage();
            return 0;
        }
        if (strcmp(a, "-a") == 0) {
            show_all = true;
            continue;
        }
        if (strcmp(a, "-L") == 0) {
            if (i + 1 >= argc) {
                usage();
                return 1;
            }
            max_depth = atoi(argv[++i]);
            if (max_depth < 0) {
                max_depth = -1;
            }
            continue;
        }
        if (a[0] == '-' && a[1] != '\0') {
            usage();
            return 1;
        }
        path = a;
    }

    struct stat st;
    if (lstat(path, &st) != 0) {
        fprintf(stderr, "tree: %s: %s\n", path, strerror(errno));
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        puts(path);
        return 0;
    }

    puts(path);
    unsigned long dirs = 0;
    unsigned long files = 0;
    print_tree(path, "", show_all, 0, max_depth, &dirs, &files);
    printf("\n%lu directories, %lu files\n", dirs, files);
    return 0;
}
