#include "syscall.h"

static uint32_t cstr_len(const char* s) {
    uint32_t n = 0;
    while (s && s[n]) {
        n++;
    }
    return n;
}

int main(void) {
    const char msg[] = "Hello from user mode (init)!\n";
    sys_write(msg, (uint32_t)(sizeof(msg) - 1u));

    const char hdr1[] = "\n[init] Reading fat/hello.txt via SYS_READFILE:\n";
    sys_write(hdr1, (uint32_t)(sizeof(hdr1) - 1u));

    char buf[128];
    int n = sys_readfile("fat/hello.txt", buf, (uint32_t)sizeof(buf), 0);
    if (n > 0) {
        sys_write(buf, (uint32_t)n);
    } else {
        const char err[] = "[init] readfile failed\n";
        sys_write(err, (uint32_t)(sizeof(err) - 1u));
    }

    const char hdr2[] = "\n[init] Reading fat/big.txt in chunks:\n";
    sys_write(hdr2, (uint32_t)(sizeof(hdr2) - 1u));

    for (uint32_t off = 0; off < 256u; off += 96u) {
        n = sys_readfile("fat/big.txt", buf, (uint32_t)sizeof(buf), off);
        if (n <= 0) {
            const char err2[] = "[init] big.txt read failed\n";
            sys_write(err2, (uint32_t)(sizeof(err2) - 1u));
            break;
        }
        const char prefix[] = "[chunk] ";
        sys_write(prefix, (uint32_t)(sizeof(prefix) - 1u));
        sys_write(buf, (uint32_t)n);
        if (buf[n - 1] != '\n') {
            const char nl[] = "\n";
            sys_write(nl, 1u);
        }
    }

    const char done[] = "\n[init] done.\n";
    sys_write(done, cstr_len(done));
    sys_exit(0);
}
