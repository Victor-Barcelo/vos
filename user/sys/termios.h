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

// c_lflag bits (subset; values chosen for compatibility with common code).
#define ISIG   0x00000001u
#define ICANON 0x00000002u
#define ECHO   0x00000008u
#define IEXTEN 0x00008000u

// c_iflag bits used by cfmakeraw/linenoise-style code.
#define BRKINT 0x00000002u
#define ICRNL  0x00000100u
#define INPCK  0x00000010u
#define ISTRIP 0x00000020u
#define IXON   0x00000400u

// c_oflag bits.
#define OPOST  0x00000001u

// c_cflag bits.
#define CS8    0x00000030u

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

