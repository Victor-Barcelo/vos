#ifndef USER_SYS_DIRENT_H
#define USER_SYS_DIRENT_H

#include <sys/types.h>

// Minimal dirent support for VOS/newlib.
// Newlib's upstream i686-elf headers ship a <dirent.h> wrapper but disable
// <sys/dirent.h> for bare-metal targets. VOS provides a small implementation
// backed by SYS_READDIR.

#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif
#ifndef DT_DIR
#define DT_DIR 4
#endif
#ifndef DT_REG
#define DT_REG 8
#endif

struct dirent {
    ino_t d_ino;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

typedef struct vos_DIR {
    int fd;
    struct dirent de;
    int eof;
} DIR;

#endif
