#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void cat_file(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("[init] open(%s) failed: %s\n", path, strerror(errno));
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

int main(void) {
    printf("Hello from user mode (init)!\n");

    printf("\n[init] Reading fat/hello.txt:\n");
    cat_file("fat/hello.txt");

    printf("\n[init] Reading fat/big.txt:\n");
    cat_file("fat/big.txt");

    printf("\n[init] Reading fat/dir/nested.txt:\n");
    cat_file("fat/dir/nested.txt");

    printf("\n[init] done.\n");
    return 0;
}
