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

    uint32_t bsize = st.bsize ? st.bsize : 1024u;
    uint64_t total = (uint64_t)st.blocks * (uint64_t)bsize;
    uint64_t freeb = (uint64_t)st.bfree * (uint64_t)bsize;
    uint64_t avail = (uint64_t)st.bavail * (uint64_t)bsize;
    uint64_t used = (total >= freeb) ? (total - freeb) : 0;
    unsigned int usep = (total == 0) ? 0u : (unsigned int)((used * 100u) / total);

    // Match common df output: 1K blocks.
    unsigned long total_k = (unsigned long)(total / 1024u);
    unsigned long used_k = (unsigned long)(used / 1024u);
    unsigned long avail_k = (unsigned long)(avail / 1024u);

    printf("%-12s %10lu %10lu %10lu %3u%% %s\n",
           path ? path : "(null)", total_k, used_k, avail_k, usep, path ? path : "(null)");
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
