#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "syscall.h"

static const char* state_str(uint32_t state) {
    switch (state) {
        case 0: return "RUN";
        case 1: return "SLEEP";
        case 2: return "WAIT";
        case 3: return "ZOMB";
        default: return "?";
    }
}

static int print_ps_once(void) {
    int count = sys_task_count();
    if (count < 0) {
        errno = -count;
        fprintf(stderr, "top: %s\n", strerror(errno));
        return -1;
    }

    int cur = getpid();
    puts("PID   USER  STATE  TICKS    EIP       NAME");
    for (uint32_t i = 0; i < (uint32_t)count; i++) {
        vos_task_info_t ti;
        if (sys_task_info(i, &ti) < 0) {
            continue;
        }

        const char* user = ti.user ? "user" : "kern";
        const char* st = state_str(ti.state);
        char mark = (ti.pid == (uint32_t)cur) ? '*' : ' ';

        printf("%c%-4lu %-5s %-5s %-8lu 0x%08lx %s\n",
               mark,
               (unsigned long)ti.pid,
               user,
               st,
               (unsigned long)ti.cpu_ticks,
               (unsigned long)ti.eip,
               ti.name);
    }
    return 0;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    struct termios orig;
    int have_termios = (tcgetattr(0, &orig) == 0);
    if (have_termios) {
        struct termios raw = orig;
        cfmakeraw(&raw);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        (void)tcsetattr(0, TCSAFLUSH, &raw);
    }

    for (;;) {
        char c = 0;
        int n = (int)read(0, &c, 1u);
        if (n == 1 && (c == 'q' || c == 'Q')) {
            break;
        }

        // Clear screen + home.
        fputs("\x1b[2J\x1b[H", stdout);
        puts("top: press 'q' to quit");
        (void)print_ps_once();
        fflush(stdout);

        (void)sys_sleep(1000u);
    }

    if (have_termios) {
        (void)tcsetattr(0, TCSAFLUSH, &orig);
    }

    return 0;
}

