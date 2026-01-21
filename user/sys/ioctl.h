#ifndef USER_SYS_IOCTL_H
#define USER_SYS_IOCTL_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

// Linux-compatible request values (widely used by existing code).
#define TCGETS     0x5401u
#define TCSETS     0x5402u
#define TCSETSW    0x5403u
#define TCSETSF    0x5404u
#define TIOCGWINSZ 0x5413u

int ioctl(int fd, unsigned long request, ...);

#ifdef __cplusplus
}
#endif

#endif

