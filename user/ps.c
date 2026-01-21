#include <errno.h>
#include <stdio.h>
#include <string.h>
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

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    int count = sys_task_count();
    if (count < 0) {
        errno = -count;
        fprintf(stderr, "ps: %s\n", strerror(errno));
        return 1;
    }

    int cur = getpid();

    puts("PID   USER  STATE  TICKS    EIP       NAME");
    for (uint32_t i = 0; i < (uint32_t)count; i++) {
        vos_task_info_t ti;
        int rc = sys_task_info(i, &ti);
        if (rc < 0) {
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

