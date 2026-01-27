#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

static void posix_selftest(void) {
    tag("[init] ", CLR_CYAN);
    printf("POSIX self-test (symlink/readlink/lstat/chmod):\n");

    // File symlink.
    (void)unlink("/tmp/link");
    if (symlink("/disk/userland.txt", "/tmp/link") < 0) {
        tag("[init] ", CLR_CYAN);
        tag("error: ", CLR_RED);
        printf("symlink(/disk/userland.txt, /tmp/link) failed: %s\n", strerror(errno));
    } else {
        char buf[256];
        ssize_t n = readlink("/tmp/link", buf, sizeof(buf) - 1u);
        if (n < 0) {
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("readlink(/tmp/link) failed: %s\n", strerror(errno));
        } else {
            buf[n] = '\0';
            tag("[init] ", CLR_CYAN);
            printf("readlink(/tmp/link) = '%s'\n", buf);
        }

        struct stat st;
        struct stat lst;
        if (stat("/tmp/link", &st) == 0 && lstat("/tmp/link", &lst) == 0) {
            tag("[init] ", CLR_CYAN);
            printf("stat(/tmp/link):  %s mode=%04o\n",
                   S_ISREG(st.st_mode) ? "file" : (S_ISDIR(st.st_mode) ? "dir" : "?"),
                   (unsigned int)(st.st_mode & 07777u));
            tag("[init] ", CLR_CYAN);
            printf("lstat(/tmp/link): %s mode=%04o\n",
                   S_ISLNK(lst.st_mode) ? "symlink" : (S_ISDIR(lst.st_mode) ? "dir" : (S_ISREG(lst.st_mode) ? "file" : "?")),
                   (unsigned int)(lst.st_mode & 07777u));
        }
    }

    // Directory symlink + intermediate traversal.
    (void)mkdir("/tmp/adir", 0777);
    (void)unlink("/tmp/adir/inner.txt");
    int fdw = open("/tmp/adir/inner.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fdw >= 0) {
        const char* msg = "Hello through a symlinked directory!\n";
        (void)write(fdw, msg, strlen(msg));
        close(fdw);
    }

    (void)unlink("/tmp/dlink");
    if (symlink("/tmp/adir", "/tmp/dlink") < 0) {
        tag("[init] ", CLR_CYAN);
        tag("error: ", CLR_RED);
        printf("symlink(/tmp/adir, /tmp/dlink) failed: %s\n", strerror(errno));
    } else {
        tag("[init] ", CLR_CYAN);
        printf("Reading /tmp/dlink/inner.txt:\n");
        cat_file("/tmp/dlink/inner.txt");
    }

    // chmod/fchmod surface.
    int fd = open("/tmp/perm.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        (void)write(fd, "x", 1u);
        if (fchmod(fd, 0) < 0) {
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("fchmod(fd, 0) failed: %s\n", strerror(errno));
        }
        close(fd);
    }
    struct stat pst;
    if (stat("/tmp/perm.txt", &pst) == 0) {
        tag("[init] ", CLR_CYAN);
        printf("stat(/tmp/perm.txt): mode=%04o\n", (unsigned int)(pst.st_mode & 07777u));
    }

    printf("\n");
}

static void posix_process_selftest(void) {
    tag("[init] ", CLR_CYAN);
    printf("POSIX process self-test (fork/waitpid/execve):\n");

    int status = 0;
    pid_t child = fork();
    if (child < 0) {
        tag("[init] ", CLR_CYAN);
        tag("error: ", CLR_RED);
        printf("fork() failed: %s\n", strerror(errno));
    } else if (child == 0) {
        _exit(42);
    } else {
        pid_t got = waitpid(child, &status, 0);
        if (got < 0) {
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("waitpid(%d) failed: %s\n", (int)child, strerror(errno));
        } else {
            tag("[init] ", CLR_CYAN);
            printf("waitpid(%d) -> %d status=0x%X exit=%d\n",
                   (int)child, (int)got, (unsigned int)status, (status >> 8) & 0xFF);
        }
    }

    pid_t sleeper = fork();
    if (sleeper < 0) {
        tag("[init] ", CLR_CYAN);
        tag("error: ", CLR_RED);
        printf("fork() for WNOHANG failed: %s\n", strerror(errno));
    } else if (sleeper == 0) {
        (void)sys_sleep(50u);
        _exit(7);
    } else {
        int rc = waitpid(sleeper, &status, 1 /* WNOHANG */);
        tag("[init] ", CLR_CYAN);
        printf("waitpid(WNOHANG) -> %d (expected 0)\n", rc);
        pid_t got = waitpid(sleeper, &status, 0);
        if (got < 0) {
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("waitpid(%d) failed: %s\n", (int)sleeper, strerror(errno));
        } else {
            tag("[init] ", CLR_CYAN);
            printf("waitpid(%d) -> %d status=0x%X exit=%d\n",
                   (int)sleeper, (int)got, (unsigned int)status, (status >> 8) & 0xFF);
        }
    }

    pid_t exec_child = fork();
    if (exec_child < 0) {
        tag("[init] ", CLR_CYAN);
        tag("error: ", CLR_RED);
        printf("fork() for execve failed: %s\n", strerror(errno));
    } else if (exec_child == 0) {
        char* const argv[] = {"echo", "execve ok", NULL};
        execve("/bin/echo", argv, NULL);
        tag("[init] ", CLR_CYAN);
        tag("error: ", CLR_RED);
        printf("execve(/bin/echo) failed: %s\n", strerror(errno));
        _exit(127);
    } else {
        pid_t got = waitpid(exec_child, &status, 0);
        if (got < 0) {
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("waitpid(%d) failed: %s\n", (int)exec_child, strerror(errno));
        } else {
            tag("[init] ", CLR_CYAN);
            printf("execve child exit=%d\n", (status >> 8) & 0xFF);
        }
    }

    printf("\n");
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
    posix_selftest();
    posix_process_selftest();

    // Clear screen (but content remains in scrollback - user can scroll up)
    // \x1b[2J = clear entire screen, \x1b[H = move cursor to home
    printf("\x1b[2J\x1b[H");
    fflush(stdout);

    // Show neofetch before login prompt
    pid_t neo_pid = fork();
    if (neo_pid == 0) {
        char* const argv[] = {"/bin/neofetch", NULL};
        execve("/bin/neofetch", argv, NULL);
        _exit(127);
    } else if (neo_pid > 0) {
        int neo_status = 0;
        (void)waitpid(neo_pid, &neo_status, 0);
    }
    printf("\n");
    fflush(stdout);

    // Keep init (PID 1) alive and supervise the user shell.
    for (;;) {
        pid_t pid = fork();
        if (pid < 0) {
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("fork() for /bin/login failed: %s\n", strerror(errno));
            (void)sys_sleep(1000u);
            continue;
        }

        if (pid == 0) {
            char* const argv[] = {"/bin/login", NULL};
            execve("/bin/login", argv, NULL);
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("execve(/bin/login) failed: %s\n", strerror(errno));
            _exit(127);
        }

        int status = 0;
        pid_t got = waitpid(pid, &status, 0);
        int code = 0;
        if (got < 0) {
            tag("[init] ", CLR_CYAN);
            tag("error: ", CLR_RED);
            printf("waitpid(%d) failed: %s\n", (int)pid, strerror(errno));
        } else {
            code = (status >> 8) & 0xFF;
        }
        tag("[init] ", CLR_CYAN);
        tag("warn: ", CLR_YELLOW);
        printf("/bin/login exited (%d), restarting...\n", code);
    }
}
