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

    const char hdr1[] = "\n[init] Reading fat/hello.txt via open/read/close:\n";
    sys_write(hdr1, (uint32_t)(sizeof(hdr1) - 1u));

    char buf[128];
    int fd = sys_open("fat/hello.txt", 0);
    if (fd >= 0) {
        int n = sys_read(fd, buf, (uint32_t)sizeof(buf));
        if (n > 0) {
            sys_write(buf, (uint32_t)n);
        } else {
            const char err[] = "[init] read failed\n";
            sys_write(err, (uint32_t)(sizeof(err) - 1u));
        }
        sys_close(fd);
    } else {
        const char err[] = "[init] open failed\n";
        sys_write(err, (uint32_t)(sizeof(err) - 1u));
    }

    const char hdr2[] = "\n[init] Reading fat/big.txt in chunks via read(fd):\n";
    sys_write(hdr2, (uint32_t)(sizeof(hdr2) - 1u));

    fd = sys_open("fat/big.txt", 0);
    if (fd >= 0) {
        for (;;) {
            int n = sys_read(fd, buf, (uint32_t)sizeof(buf));
            if (n <= 0) {
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
        sys_close(fd);
    } else {
        const char err2[] = "[init] big.txt open failed\n";
        sys_write(err2, (uint32_t)(sizeof(err2) - 1u));
    }

    const char hdr3[] = "\n[init] Reading fat/dir/nested.txt:\n";
    sys_write(hdr3, (uint32_t)(sizeof(hdr3) - 1u));

    fd = sys_open("fat/dir/nested.txt", 0);
    if (fd >= 0) {
        int n = sys_read(fd, buf, (uint32_t)sizeof(buf));
        if (n > 0) {
            sys_write(buf, (uint32_t)n);
        } else {
            const char err3[] = "[init] nested.txt read failed\n";
            sys_write(err3, (uint32_t)(sizeof(err3) - 1u));
        }
        sys_close(fd);
    } else {
        const char err3[] = "[init] nested.txt open failed\n";
        sys_write(err3, (uint32_t)(sizeof(err3) - 1u));
    }

    const char done[] = "\n[init] done.\n";
    sys_write(done, cstr_len(done));
    sys_exit(0);
}
