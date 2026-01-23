#ifndef USER_SYS_TERMIOS_H
#define USER_SYS_TERMIOS_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

// c_cc indices (Linux-compatible).
#define VINTR  0
#define VQUIT  1
#define VERASE 2
#define VKILL  3
#define VEOF   4
#define VTIME  5
#define VMIN   6
#define VSTART 8
#define VSTOP  9
#define VSUSP  10

// c_lflag bits (subset; values chosen for compatibility with common code).
#define ISIG   0x00000001u
#define ICANON 0x00000002u
#define ECHO   0x00000008u
#define ECHONL 0x00000040u
#define IEXTEN 0x00008000u

// c_iflag bits used by cfmakeraw/linenoise-style code.
#define IGNBRK 0x00000001u
#define BRKINT 0x00000002u
#define ICRNL  0x00000100u
#define INLCR  0x00000040u
#define INPCK  0x00000010u
#define ISTRIP 0x00000020u
#define IXON   0x00000400u
#define IXOFF  0x00001000u

// c_oflag bits.
#define OPOST  0x00000001u

// c_cflag bits.
#define CSIZE  0x00000030u
#define PARENB 0x00000100u
#define CS8    0x00000030u

// _POSIX_VDISABLE is used to disable a special character (e.g. VSUSP).
#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0
#endif

// Common baud rates (enough for ports that inspect cfgetospeed()).
#define B0     0
#define B50    50
#define B75    75
#define B110   110
#define B134   134
#define B150   150
#define B200   200
#define B300   300
#define B600   600
#define B1200  1200
#define B1800  1800
#define B2400  2400
#define B4800  4800
#define B9600  9600
#define B19200 19200
#define B38400 38400

static inline speed_t cfgetospeed(const struct termios* t) {
    return t ? t->c_ospeed : (speed_t)0;
}

static inline speed_t cfgetispeed(const struct termios* t) {
    return t ? t->c_ispeed : (speed_t)0;
}

static inline int cfsetospeed(struct termios* t, speed_t speed) {
    if (!t) {
        return -1;
    }
    t->c_ospeed = speed;
    return 0;
}

static inline int cfsetispeed(struct termios* t, speed_t speed) {
    if (!t) {
        return -1;
    }
    t->c_ispeed = speed;
    return 0;
}

// tcsetattr actions.
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

int tcgetattr(int fd, struct termios* termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios* termios_p);
void cfmakeraw(struct termios* t);

#ifdef __cplusplus
}
#endif

#endif
