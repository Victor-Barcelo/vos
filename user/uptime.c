#include <stdio.h>
#include <stdint.h>

#include "syscall.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    uint32_t uptime_ms = sys_uptime_ms();
    uint32_t seconds = uptime_ms / 1000u;
    uint32_t ms = uptime_ms % 1000u;

    printf("Uptime: %lu.%03lus\n", (unsigned long)seconds, (unsigned long)ms);
    return 0;
}

