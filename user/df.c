#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "syscall.h"

static void print_errno(const char* path, int rc) {
    if (rc >= 0) {
        return;
    }
    errno = -rc;
    fprintf(stderr, "df: %s: %s\n", path ? path : "(null)", strerror(errno));
}

static void print_one(const char* path) {
    vos_statfs_t st;
    int rc = sys_statfs(path, &st);
    if (rc < 0) {
        print_errno(path, rc);
        return;
    }

    unsigned long long total = (unsigned long long)st.blocks * (unsigned long long)st.bsize;
    unsigned long long freeb = (unsigned long long)st.bfree * (unsigned long long)st.bsize;
    unsigned long long avail = (unsigned long long)st.bavail * (unsigned long long)st.bsize;
    unsigned long long used = (total >= freeb) ? (total - freeb) : 0;
    unsigned int usep = (total == 0) ? 0u : (unsigned int)((used * 100ull) / total);

    // Match common df output: 1K blocks.
    unsigned long long total_k = total / 1024ull;
    unsigned long long used_k = used / 1024ull;
    unsigned long long avail_k = avail / 1024ull;

    printf("%-12s %10llu %10llu %10llu %3u%% %s\n",
           path, total_k, used_k, avail_k, usep, path);
}

int main(int argc, char** argv) {
    puts("Filesystem   1K-blocks       Used  Available Use% Mounted on");

    if (argc <= 1) {
        print_one("/");
        print_one("/disk");
        print_one("/ram");
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i] && argv[i][0] != '\0') {
            print_one(argv[i]);
        }
    }
    return 0;
}
