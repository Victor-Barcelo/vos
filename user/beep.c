#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "syscall.h"

int main(int argc, char** argv) {
    uint32_t freq = 440;      // Default: A4 (440 Hz)
    uint32_t duration = 200;  // Default: 200 ms

    if (argc >= 2) {
        freq = (uint32_t)atoi(argv[1]);
        if (freq == 0) {
            freq = 440;
        }
    }

    if (argc >= 3) {
        duration = (uint32_t)atoi(argv[2]);
        if (duration == 0) {
            duration = 200;
        }
    }

    sys_beep(freq, duration);
    return 0;
}
