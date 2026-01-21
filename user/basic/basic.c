#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "basic_programs.h"
#include "ubasic.h"

static char* read_entire_file(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    // Try to stat for sizing.
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        (void)lseek(fd, 0, SEEK_SET);
        size = 0;
    } else {
        (void)lseek(fd, 0, SEEK_SET);
    }

    size_t cap = (size > 0) ? (size_t)size : 4096u;
    char* buf = (char*)malloc(cap + 1u);
    if (!buf) {
        close(fd);
        return NULL;
    }

    size_t used = 0;
    for (;;) {
        if (used == cap) {
            size_t new_cap = cap * 2u;
            char* nb = (char*)realloc(buf, new_cap + 1u);
            if (!nb) {
                free(buf);
                close(fd);
                return NULL;
            }
            buf = nb;
            cap = new_cap;
        }

        int n = (int)read(fd, buf + used, (unsigned int)(cap - used));
        if (n < 0) {
            free(buf);
            close(fd);
            return NULL;
        }
        if (n == 0) {
            break;
        }
        used += (size_t)n;
    }

    buf[used] = '\0';
    close(fd);
    return buf;
}

static void run_program(const char* program) {
    ubasic_init(program);
    while (!ubasic_finished()) {
        ubasic_run();
    }
}

static void list_demos(void) {
    puts("=== BASIC demo programs ===");
    for (int i = 1; i <= BASIC_NUM_PROGRAMS; i++) {
        const char* name = basic_get_program_name(i);
        const char* desc = basic_get_program_description(i);
        if (!name) name = "(unknown)";
        if (!desc) desc = "";
        printf("%2d. %s - %s\n", i, name, desc);
    }
}

int main(int argc, char** argv) {
    if (argc >= 2) {
        if (strcmp(argv[1], "-d") == 0 && argc >= 3) {
            int idx = atoi(argv[2]);
            const char* demo = basic_get_program(idx);
            if (!demo) {
                fprintf(stderr, "basic: unknown demo %d\n", idx);
                return 1;
            }
            run_program(demo);
            return 0;
        }

        char* program = read_entire_file(argv[1]);
        if (!program) {
            fprintf(stderr, "basic: open %s failed: %s\n", argv[1], strerror(errno));
            return 1;
        }
        run_program(program);
        free(program);
        return 0;
    }

    list_demos();
    puts("");
    puts("Run a demo with: basic -d <n>");
    puts("Or run a .bas file with: basic <file.bas>");
    return 0;
}

