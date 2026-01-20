#include "syscall.h"

int main(void) {
    const char msg[] = "Hello from user mode (init)!\n";
    sys_write(msg, (uint32_t)(sizeof(msg) - 1u));
    sys_exit(0);
}
