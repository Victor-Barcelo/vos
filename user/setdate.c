#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "syscall.h"

static void usage(void) {
    puts("Usage: setdate <YYYY-MM-DD HH:MM:SS>");
    puts("   or: setdate <YYYY-MM-DDTHH:MM:SS>");
}

static int parse_ndigits(const char** p, int n, int* out) {
    int value = 0;
    for (int i = 0; i < n; i++) {
        char c = (*p)[i];
        if (c < '0' || c > '9') {
            return 0;
        }
        value = value * 10 + (c - '0');
    }
    *p += n;
    *out = value;
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    // Accept "YYYY-MM-DD HH:MM:SS" split across argv, or combined with 'T'.
    char buf[64];
    buf[0] = '\0';

    if (argc >= 3) {
        snprintf(buf, sizeof(buf), "%s %s", argv[1], argv[2]);
    } else {
        strncpy(buf, argv[1], sizeof(buf) - 1u);
        buf[sizeof(buf) - 1u] = '\0';
    }

    const char* p = buf;
    while (*p == ' ' || *p == '\t') p++;

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (!parse_ndigits(&p, 4, &year) || *p++ != '-' ||
        !parse_ndigits(&p, 2, &month) || *p++ != '-' ||
        !parse_ndigits(&p, 2, &day)) {
        usage();
        return 1;
    }

    if (*p == 'T') {
        p++;
    } else if (*p == ' ') {
        while (*p == ' ') p++;
    } else {
        usage();
        return 1;
    }

    if (!parse_ndigits(&p, 2, &hour) || *p++ != ':' ||
        !parse_ndigits(&p, 2, &minute) || *p++ != ':' ||
        !parse_ndigits(&p, 2, &second)) {
        usage();
        return 1;
    }

    vos_rtc_datetime_t dt;
    dt.year = (uint16_t)year;
    dt.month = (uint8_t)month;
    dt.day = (uint8_t)day;
    dt.hour = (uint8_t)hour;
    dt.minute = (uint8_t)minute;
    dt.second = (uint8_t)second;

    int rc = sys_rtc_set(&dt);
    if (rc < 0) {
        errno = -rc;
        fprintf(stderr, "setdate: %s\n", strerror(errno));
        return 1;
    }

    puts("RTC updated.");
    return 0;
}

