/*
 * dirent.h - Directory entry header for VOS
 *
 * This header declares the directory functions and types for VOS.
 * The actual implementation is in newlib_syscalls.c
 */
#ifndef _DIRENT_H
#define _DIRENT_H

#include <sys/types.h>

/* Maximum filename length */
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

/* Directory entry types */
#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK      10
#define DT_SOCK     12
#define DT_WHT      14

/* Directory entry structure */
struct dirent {
    unsigned long d_ino;            /* Inode number */
    unsigned short d_reclen;        /* Length of this record */
    unsigned char d_type;           /* Type of file */
    char d_name[NAME_MAX + 1];      /* Filename (null-terminated) */
};

/* Opaque directory stream type */
typedef struct _DIR DIR;

/* Internal DIR structure - match newlib_syscalls.c */
struct _DIR {
    int fd;                         /* Directory file descriptor */
    int eof;                        /* End of directory flag */
    struct dirent de;               /* Current entry */
};

#ifdef __cplusplus
extern "C" {
#endif

/* Open a directory stream */
DIR *opendir(const char *name);

/* Open a directory stream from file descriptor */
DIR *fdopendir(int fd);

/* Read a directory entry */
struct dirent *readdir(DIR *dirp);

/* Close a directory stream */
int closedir(DIR *dirp);

/* Rewind directory stream to beginning */
void rewinddir(DIR *dirp);

/* Seek to position in directory stream */
void seekdir(DIR *dirp, long loc);

/* Return current position in directory stream */
long telldir(DIR *dirp);

/* Return the file descriptor for the directory stream */
int dirfd(DIR *dirp);

#ifdef __cplusplus
}
#endif

#endif /* _DIRENT_H */
