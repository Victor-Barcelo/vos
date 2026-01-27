/*
 * unzip - extract ZIP archives using miniz
 * Usage: unzip archive.zip [-d dir]
 */

#define MINIZ_NO_TIME
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "../third_party/miniz/miniz.h"
#include "../third_party/miniz/miniz.c"

static void usage(void) {
    fprintf(stderr, "Usage: unzip archive.zip [-d dir] [-l]\n");
    fprintf(stderr, "  -d dir  extract to directory\n");
    fprintf(stderr, "  -l      list contents only\n");
    fprintf(stderr, "  -h      show this help\n");
}

/* Create directory and parents if needed */
static int mkdirp(const char *path) {
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

/* Ensure parent directory exists */
static void ensure_parent_dir(const char *filepath) {
    char *tmp = strdup(filepath);
    if (!tmp) return;

    char *slash = strrchr(tmp, '/');
    if (slash && slash != tmp) {
        *slash = 0;
        mkdirp(tmp);
    }
    free(tmp);
}

static int list_zip(const char *zipname) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, zipname, 0)) {
        fprintf(stderr, "unzip: cannot open '%s'\n", zipname);
        return 1;
    }

    mz_uint num_files = mz_zip_reader_get_num_files(&zip);

    printf("Archive: %s\n", zipname);
    printf("  Length      Name\n");
    printf("---------  ----\n");

    mz_uint64 total = 0;
    for (mz_uint i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            fprintf(stderr, "unzip: cannot stat file %u\n", i);
            continue;
        }

        printf("%9llu  %s%s\n",
               (unsigned long long)stat.m_uncomp_size,
               stat.m_filename,
               stat.m_is_directory ? "/" : "");
        total += stat.m_uncomp_size;
    }

    printf("---------  ----\n");
    printf("%9llu  %u file(s)\n", (unsigned long long)total, num_files);

    mz_zip_reader_end(&zip);
    return 0;
}

static int extract_zip(const char *zipname, const char *destdir) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, zipname, 0)) {
        fprintf(stderr, "unzip: cannot open '%s'\n", zipname);
        return 1;
    }

    mz_uint num_files = mz_zip_reader_get_num_files(&zip);

    printf("Archive: %s\n", zipname);

    int errors = 0;
    for (mz_uint i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            fprintf(stderr, "unzip: cannot stat file %u\n", i);
            errors++;
            continue;
        }

        char outpath[512];
        if (destdir) {
            snprintf(outpath, sizeof(outpath), "%s/%s", destdir, stat.m_filename);
        } else {
            snprintf(outpath, sizeof(outpath), "%s", stat.m_filename);
        }

        if (stat.m_is_directory) {
            printf("   creating: %s\n", outpath);
            mkdirp(outpath);
            continue;
        }

        printf("  inflating: %s\n", outpath);

        ensure_parent_dir(outpath);

        size_t uncomp_size;
        void *data = mz_zip_reader_extract_to_heap(&zip, i, &uncomp_size, 0);
        if (!data) {
            fprintf(stderr, "unzip: failed to extract '%s'\n", stat.m_filename);
            errors++;
            continue;
        }

        FILE *f = fopen(outpath, "wb");
        if (!f) {
            fprintf(stderr, "unzip: cannot create '%s': %s\n", outpath, strerror(errno));
            free(data);
            errors++;
            continue;
        }

        if (uncomp_size > 0) {
            if (fwrite(data, 1, uncomp_size, f) != uncomp_size) {
                fprintf(stderr, "unzip: write error on '%s'\n", outpath);
                errors++;
            }
        }

        fclose(f);
        free(data);
    }

    mz_zip_reader_end(&zip);

    if (errors) {
        fprintf(stderr, "unzip: completed with %d error(s)\n", errors);
        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    const char *zipname = NULL;
    const char *destdir = NULL;
    int list_only = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[i], "-l") == 0) {
            list_only = 1;
        } else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "unzip: -d requires directory argument\n");
                return 1;
            }
            destdir = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "unzip: unknown option '%s'\n", argv[i]);
            usage();
            return 1;
        } else {
            zipname = argv[i];
        }
    }

    if (!zipname) {
        fprintf(stderr, "unzip: no archive specified\n");
        usage();
        return 1;
    }

    if (destdir) {
        mkdirp(destdir);
    }

    if (list_only) {
        return list_zip(zipname);
    } else {
        return extract_zip(zipname, destdir);
    }
}
