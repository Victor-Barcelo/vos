#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "linenoise.h"
#include "syscall.h"

#define SHELL_MAX_LINE 512
#define SHELL_MAX_ARGS 32

typedef struct vos_dirent {
    char name[64];
    unsigned char is_dir;
    unsigned char _pad[3];
    unsigned int size;
} vos_dirent_t;

static int sys_readdir(int fd, vos_dirent_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READDIR), "b"(fd), "c"(out)
        : "memory"
    );
    return ret;
}

static void print_errno(const char* what) {
    if (!what) what = "error";
    printf("%s: %s\n", what, strerror(errno));
}

static int split_args(char* line, char* argv[], int max) {
    int argc = 0;
    char* p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        if (argc >= max) break;

        char quote = 0;
        if (*p == '"' || *p == '\'') {
            quote = *p++;
        }

        argv[argc++] = p;

        while (*p) {
            if (quote) {
                if (*p == quote) {
                    *p++ = '\0';
                    break;
                }
            } else {
                if (*p == ' ' || *p == '\t') {
                    *p++ = '\0';
                    break;
                }
            }
            p++;
        }
    }

    return argc;
}

static void cmd_help(void) {
    puts("Built-ins:");
    puts("  help                Show this help");
    puts("  exit                Exit the shell");
    puts("  cd [dir]            Change directory");
    puts("  pwd                 Print current directory");
    puts("  ls [dir]            List directory");
    puts("  cat <file>          Print a file");
    puts("  clear               Clear the screen");

    puts("");
    puts("Programs in /bin:");

    int fd = open("/bin", O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        print_errno("open /bin");
        return;
    }

    char names[64][64];
    int count = 0;

    vos_dirent_t de;
    while (count < (int)(sizeof(names) / sizeof(names[0])) && sys_readdir(fd, &de) > 0) {
        if (de.name[0] == '\0') continue;
        if (de.is_dir) continue;
        strncpy(names[count], de.name, sizeof(names[count]) - 1u);
        names[count][sizeof(names[count]) - 1u] = '\0';
        count++;
    }
    close(fd);

    // Simple insertion sort for a stable, predictable list.
    for (int i = 1; i < count; i++) {
        char tmp[64];
        strncpy(tmp, names[i], sizeof(tmp));
        tmp[sizeof(tmp) - 1u] = '\0';

        int j = i;
        while (j > 0 && strcmp(names[j - 1], tmp) > 0) {
            strncpy(names[j], names[j - 1], sizeof(names[j]));
            names[j][sizeof(names[j]) - 1u] = '\0';
            j--;
        }
        strncpy(names[j], tmp, sizeof(names[j]));
        names[j][sizeof(names[j]) - 1u] = '\0';
    }

    for (int i = 0; i < count; i++) {
        printf("  %s\n", names[i]);
    }
}

static void cmd_pwd(void) {
    char buf[256];
    if (!getcwd(buf, sizeof(buf))) {
        print_errno("getcwd");
        return;
    }
    puts(buf);
}

static void cmd_cd(int argc, char** argv) {
    const char* dir = (argc >= 2) ? argv[1] : "/";
    if (chdir(dir) < 0) {
        print_errno("cd");
    }
}

static void cmd_clear(void) {
    // VT100 clear screen + home.
    fputs("\x1b[2J\x1b[H", stdout);
}

static void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        puts("Usage: cat <file>");
        return;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        print_errno("cat");
        return;
    }

    char buf[256];
    for (;;) {
        int n = (int)read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        (void)write(1, buf, (unsigned int)n);
    }
    close(fd);
}

static int is_elf_file(const char* dir, const char* name) {
    if (!name || name[0] == '\0') {
        return 0;
    }

    char full[512];
    if (!dir || dir[0] == '\0' || strcmp(dir, ".") == 0) {
        snprintf(full, sizeof(full), "%s", name);
    } else {
        char d[256];
        strncpy(d, dir, sizeof(d) - 1u);
        d[sizeof(d) - 1u] = '\0';
        size_t len = strlen(d);
        while (len > 1 && d[len - 1u] == '/') {
            d[len - 1u] = '\0';
            len--;
        }
        if (strcmp(d, "/") == 0) {
            snprintf(full, sizeof(full), "/%s", name);
        } else {
            snprintf(full, sizeof(full), "%s/%s", d, name);
        }
    }

    int fd = open(full, O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    unsigned char hdr[4];
    int n = (int)read(fd, hdr, sizeof(hdr));
    close(fd);
    if (n != (int)sizeof(hdr)) {
        return 0;
    }
    return (hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F') ? 1 : 0;
}

static void cmd_ls(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] : ".";

    const char* CLR_RESET = "\x1b[0m";
    const char* CLR_DIR = "\x1b[94m";   // bright blue
    const char* CLR_EXEC = "\x1b[92m";  // bright green (used for /bin entries)

    char cwd[256];
    if (!getcwd(cwd, sizeof(cwd))) {
        strcpy(cwd, "/");
    }

    char abs[512];
    const char* list_dir = NULL;
    if (path[0] == '/') {
        list_dir = path;
    } else if (strcmp(path, ".") == 0) {
        list_dir = cwd;
    } else {
        if (strcmp(cwd, "/") == 0) {
            snprintf(abs, sizeof(abs), "/%s", path);
        } else {
            snprintf(abs, sizeof(abs), "%s/%s", cwd, path);
        }
        list_dir = abs;
    }

    // Trim trailing slashes for comparisons like "/bin/".
    char list_dir_trim[512];
    strncpy(list_dir_trim, list_dir, sizeof(list_dir_trim) - 1u);
    list_dir_trim[sizeof(list_dir_trim) - 1u] = '\0';
    size_t tlen = strlen(list_dir_trim);
    while (tlen > 1 && list_dir_trim[tlen - 1u] == '/') {
        list_dir_trim[tlen - 1u] = '\0';
        tlen--;
    }

    int in_bin = (strcmp(list_dir_trim, "/bin") == 0);

    struct stat st;
    if (stat(path, &st) < 0) {
        print_errno("ls");
        return;
    }

    if (!S_ISDIR(st.st_mode)) {
        // When listing a file, colorize it if it's under /bin.
        if (strncmp(list_dir_trim, "/bin/", 5) == 0 ||
            (strcmp(cwd, "/bin") == 0 && strchr(path, '/') == NULL) ||
            is_elf_file(".", path)) {
            printf("%s%s%s\n", CLR_EXEC, path, CLR_RESET);
        } else {
            puts(path);
        }
        return;
    }

    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        print_errno("ls");
        return;
    }

    vos_dirent_t de;
    while (sys_readdir(fd, &de) > 0) {
        if (de.name[0] == '\0') continue;
        if (de.is_dir) {
            printf("%s%s/%s\n", CLR_DIR, de.name, CLR_RESET);
        } else {
            if (in_bin || (de.size >= 4u && is_elf_file(path, de.name))) {
                printf("%s%s%s\n", CLR_EXEC, de.name, CLR_RESET);
            } else {
                printf("%s\n", de.name);
            }
        }
    }

    close(fd);
}

static void complete_first_word(const char* word, linenoiseCompletions* lc) {
    static const char* builtins[] = {"help", "exit", "cd", "pwd", "ls", "cat", "clear"};

    size_t n = strlen(word);
    for (unsigned int i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        const char* b = builtins[i];
        if (strncmp(b, word, n) == 0) {
            linenoiseAddCompletion(lc, b);
        }
    }

    // Complete executables from /bin.
    const char* prefix = word;
    const char* base = "";
    char tmp[128];
    if (strncmp(word, "/bin/", 5) == 0) {
        prefix = word + 5;
        base = "/bin/";
    }

    int fd = open("/bin", O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        vos_dirent_t de;
        while (sys_readdir(fd, &de) > 0) {
            if (de.is_dir) continue;
            if (de.name[0] == '\0') continue;
            if (strncmp(de.name, prefix, strlen(prefix)) != 0) continue;
            snprintf(tmp, sizeof(tmp), "%s%s", base, de.name);
            linenoiseAddCompletion(lc, tmp);
        }
        close(fd);
    }
}

static void completion_cb(const char* buf, linenoiseCompletions* lc) {
    if (!buf || !lc) return;

    const char* p = buf;
    while (*p == ' ' || *p == '\t') p++;

    // Only complete the first word for now.
    for (const char* q = p; *q; q++) {
        if (*q == ' ' || *q == '\t') {
            return;
        }
    }

    complete_first_word(p, lc);
}

static int run_external(int argc, char** argv) {
    const char* cmd = argv[0];
    const char* sargv[SHELL_MAX_ARGS];
    for (int i = 0; i < argc; i++) {
        sargv[i] = argv[i];
    }

    char pathbuf[256];
    const char* exec_path = cmd;

    // If user didn't specify a path, try relative first, then /bin/<cmd>.
    int pid = sys_spawn(exec_path, sargv, (uint32_t)argc);
    if (pid < 0 && strchr(cmd, '/') == NULL) {
        snprintf(pathbuf, sizeof(pathbuf), "/bin/%s", cmd);
        exec_path = pathbuf;
        sargv[0] = exec_path;
        pid = sys_spawn(exec_path, sargv, (uint32_t)argc);
    }

    if (pid < 0) {
        errno = -pid;
        print_errno(cmd);
        return -1;
    }

    int code = sys_wait((uint32_t)pid);
    printf("exit %d\n", code);
    return 0;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    linenoiseSetCompletionCallback(completion_cb);
    linenoiseHistorySetMaxLen(128);

    puts("VOS user shell (linenoise). Type 'help' for help.");

    char cwd[256];
    for (;;) {
        if (!getcwd(cwd, sizeof(cwd))) {
            strcpy(cwd, "/");
        }

        char prompt[320];
        snprintf(prompt, sizeof(prompt), "vos:%s$ ", cwd);

        char* line = linenoise(prompt);
        if (!line) {
            break;
        }

        // Trim leading whitespace.
        char* s = line;
        while (*s == ' ' || *s == '\t') s++;

        if (*s == '\0') {
            free(line);
            continue;
        }

        linenoiseHistoryAdd(s);

        char buf[SHELL_MAX_LINE];
        strncpy(buf, s, sizeof(buf) - 1u);
        buf[sizeof(buf) - 1u] = '\0';

        char* av[SHELL_MAX_ARGS];
        int ac = split_args(buf, av, SHELL_MAX_ARGS);
        if (ac <= 0) {
            free(line);
            continue;
        }

        if (strcmp(av[0], "exit") == 0) {
            free(line);
            break;
        } else if (strcmp(av[0], "help") == 0) {
            cmd_help();
        } else if (strcmp(av[0], "cd") == 0) {
            cmd_cd(ac, av);
        } else if (strcmp(av[0], "pwd") == 0) {
            cmd_pwd();
        } else if (strcmp(av[0], "ls") == 0) {
            cmd_ls(ac, av);
        } else if (strcmp(av[0], "cat") == 0) {
            cmd_cat(ac, av);
        } else if (strcmp(av[0], "clear") == 0) {
            cmd_clear();
        } else {
            (void)run_external(ac, av);
        }

        free(line);
    }

    return 0;
}
