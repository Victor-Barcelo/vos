/* Minimal dirent.h stub for VOS - directory functions not supported */
#ifndef _DIRENT_H
#define _DIRENT_H

struct dirent {
    char d_name[256];
};

typedef struct {
    int _dummy;
} DIR;

static inline DIR *opendir(const char *name) {
    (void)name;
    return (DIR*)0;  /* Always fail - no directory support */
}

static inline struct dirent *readdir(DIR *dirp) {
    (void)dirp;
    return (struct dirent*)0;
}

static inline int closedir(DIR *dirp) {
    (void)dirp;
    return 0;
}

#endif /* _DIRENT_H */
