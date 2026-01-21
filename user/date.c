#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "syscall.h"

static void print_2d(unsigned int v) {
    putchar((int)('0' + ((v / 10u) % 10u)));
    putchar((int)('0' + (v % 10u)));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    vos_rtc_datetime_t dt;
    int rc = sys_rtc_get(&dt);
    if (rc < 0) {
        errno = -rc;
        fprintf(stderr, "date: %s\n", strerror(errno));
        return 1;
    }

    printf("%u-", (unsigned int)dt.year);
    print_2d(dt.month);
    putchar('-');
    print_2d(dt.day);
    putchar(' ');
    print_2d(dt.hour);
    putchar(':');
    print_2d(dt.minute);
    putchar(':');
    print_2d(dt.second);
    putchar('\n');

    return 0;
}

