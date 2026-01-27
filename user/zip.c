/*
 * zip - create ZIP archives using miniz
 * Usage: zip archive.zip file1 [file2 ...]
 */

#define MINIZ_NO_TIME
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "../third_party/miniz/miniz.h"
#include "../third_party/miniz/miniz.c"

static void usage(void) {
    fprintf(stderr, "Usage: zip archive.zip file1 [file2 ...]\n");
    fprintf(stderr, "  -r    recurse into directories\n");
    fprintf(stderr, "  -h    show this help\n");
}

static int add_file_to_zip(mz_zip_archive *zip, const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "zip: cannot open '%s': %s\n", filepath, strerror(errno));
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fprintf(stderr, "zip: cannot get size of '%s'\n", filepath);
        fclose(f);
        return -1;
    }

    void *data = NULL;
    if (size > 0) {
        data = malloc((size_t)size);
        if (!data) {
            fprintf(stderr, "zip: out of memory\n");
            fclose(f);
            return -1;
        }
        if (fread(data, 1, (size_t)size, f) != (size_t)size) {
            fprintf(stderr, "zip: read error on '%s'\n", filepath);
            free(data);
            fclose(f);
            return -1;
        }
    }
    fclose(f);

    /* Use filename without leading path for archive name */
    const char *arcname = filepath;
    if (arcname[0] == '.' && arcname[1] == '/') arcname += 2;

    mz_bool ok = mz_zip_writer_add_mem(zip, arcname, data, (size_t)size, MZ_DEFAULT_COMPRESSION);
    free(data);

    if (!ok) {
        fprintf(stderr, "zip: failed to add '%s' to archive\n", filepath);
        return -1;
    }

    printf("  adding: %s\n", arcname);
    return 0;
}

static int add_dir_to_zip(mz_zip_archive *zip, const char *dirpath, int recursive);

static int add_entry_to_zip(mz_zip_archive *zip, const char *path, int recursive) {
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "zip: cannot stat '%s': %s\n", path, strerror(errno));
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (recursive) {
            return add_dir_to_zip(zip, path, recursive);
        } else {
            fprintf(stderr, "zip: '%s' is a directory (use -r)\n", path);
            return -1;
        }
    } else if (S_ISREG(st.st_mode)) {
        return add_file_to_zip(zip, path);
    } else {
        fprintf(stderr, "zip: skipping special file '%s'\n", path);
        return 0;
    }
}

static int add_dir_to_zip(mz_zip_archive *zip, const char *dirpath, int recursive) {
    DIR *d = opendir(dirpath);
    if (!d) {
        fprintf(stderr, "zip: cannot open directory '%s': %s\n", dirpath, strerror(errno));
        return -1;
    }

    int errors = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, de->d_name);

        if (add_entry_to_zip(zip, fullpath, recursive) != 0)
            errors++;
    }

    closedir(d);
    return errors ? -1 : 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage();
        return 1;
    }

    int recursive = 0;
    int argstart = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            recursive = 1;
            argstart = i + 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        } else {
            break;
        }
    }

    if (argc - argstart < 2) {
        usage();
        return 1;
    }

    const char *zipname = argv[argstart];

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_writer_init_file(&zip, zipname, 0)) {
        fprintf(stderr, "zip: cannot create '%s'\n", zipname);
        return 1;
    }

    printf("creating: %s\n", zipname);

    int errors = 0;
    for (int i = argstart + 1; i < argc; i++) {
        if (add_entry_to_zip(&zip, argv[i], recursive) != 0)
            errors++;
    }

    if (!mz_zip_writer_finalize_archive(&zip)) {
        fprintf(stderr, "zip: failed to finalize archive\n");
        mz_zip_writer_end(&zip);
        return 1;
    }

    mz_zip_writer_end(&zip);

    if (errors) {
        fprintf(stderr, "zip: completed with %d error(s)\n", errors);
        return 1;
    }

    return 0;
}
