#ifndef KERRNO_H
#define KERRNO_H

// Minimal POSIX errno values used by the kernel syscall layer.
// These are aligned with newlib's `sys/errno.h` in the i686-elf toolchain.

#define EPERM   1
#define ENOENT  2
#define ESRCH   3
#define EINTR   4
#define EIO     5
#define E2BIG   7
#define ENOEXEC 8
#define EBADF   9
#define ECHILD  10
#define EAGAIN  11
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EEXIST  17
#define EXDEV   18
#define ENODEV  19
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define ENFILE  23
#define EMFILE  24
#define ENOTTY  25
#define ESPIPE  29
#define EROFS   30
#define EPIPE   32
#define ERANGE  34
#define ENOSYS  88
#define ENOTEMPTY 90
#define ENAMETOOLONG 91
#define ELOOP   92
#define EOVERFLOW 139

#endif
