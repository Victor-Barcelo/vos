#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "syscall.h"

// Simple VT100 color helpers (VOS framebuffer console supports basic SGR).
#define CLR_RESET "\x1b[0m"
#define CLR_CYAN  "\x1b[36;1m"
#define CLR_GREEN "\x1b[32;1m"
#define CLR_YELLOW "\x1b[33;1m"
#define CLR_RED   "\x1b[31;1m"

static void tag(const char* t, const char* clr) {
    if (!t) t = "";
    if (!clr) clr = CLR_RESET;
    printf("%s%s%s", clr, t, CLR_RESET);
}

static void cat_file(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        tag("[init] ", CLR_CYAN);
        tag("error: ", CLR_RED);
        printf("open(%s) failed: %s\n", path, strerror(errno));
        return;
    }

    char buf[128];
    for (;;) {
        int n = (int)read(fd, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        (void)write(1, buf, (unsigned int)n);
    }

    close(fd);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    tag("Hello from user mode (init)!\n", CLR_GREEN);

    printf("\n");
    tag("[init] ", CLR_CYAN);
    printf("Reading fat/hello.txt:\n");
    cat_file("fat/hello.txt");

    printf("\n");
    tag("[init] ", CLR_CYAN);
    printf("Reading fat/big.txt:\n");
    cat_file("fat/big.txt");

    printf("\n");
    tag("[init] ", CLR_CYAN);
    printf("Reading fat/dir/nested.txt:\n");
    cat_file("fat/dir/nested.txt");

    printf("\n");
    tag("[init] ", CLR_CYAN);
    printf("Persistent /disk test:\n");
    int fdw = open("/disk/userland.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fdw < 0) {
        tag("[init] ", CLR_CYAN);
        tag("error: ", CLR_RED);
        printf("open(/disk/userland.txt) failed: %s\n", strerror(errno));
    } else {
        const char* msg = "Hello from userland on persistent /disk!\n";
        (void)write(fdw, msg, strlen(msg));
        close(fdw);

        tag("[init] ", CLR_CYAN);
        printf("Reading back /disk/userland.txt:\n");
        cat_file("/disk/userland.txt");
    }

    printf("\n");
    tag("[init] ", CLR_CYAN);
    tag("done.\n", CLR_GREEN);

    // Linux-like temp directory (mapped to RAM via the VFS alias).
    (void)mkdir("/tmp", 0777);

    // Keep init (PID 1) alive and supervise the user shell.
    for (;;) {
        const char* const login_argv[] = {"/bin/login"};
        int pid = sys_spawn("/bin/login", login_argv, 1u);
        if (pid < 0) {
            errno = -pid;
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("spawn /bin/login failed: %s\n", strerror(errno));
            (void)sys_sleep(1000u);
            continue;
        }

        int code = sys_wait((uint32_t)pid);
        tag("[init] ", CLR_CYAN);
        tag("warn: ", CLR_YELLOW);
        printf("/bin/login exited (%d), restarting...\n", code);
    }
}
