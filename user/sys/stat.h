#ifndef VOS_USER_SYS_STAT_H
#define VOS_USER_SYS_STAT_H

// newlib's bare-metal sys/stat.h can be missing a few POSIX prototypes that
// common ports (like sbase) rely on. Provide them here while deferring the
// actual struct/stat macros to the toolchain header.

#include_next <sys/stat.h>

#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

int lstat(const char* path, struct stat* st);
int fstatat(int dirfd, const char* path, struct stat* st, int flags);
int openat(int dirfd, const char* path, int flags, ...);
int utimensat(int dirfd, const char* path, const struct timespec times[2], int flags);
int futimens(int fd, const struct timespec times[2]);

int chmod(const char* path, mode_t mode);
int fchmod(int fd, mode_t mode);

int chown(const char* path, uid_t owner, gid_t group);
int lchown(const char* path, uid_t owner, gid_t group);

int mknod(const char* path, mode_t mode, dev_t dev);

#ifdef __cplusplus
}
#endif

#endif
