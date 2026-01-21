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
    unsigned short wtime;
    unsigned short wdate;
} vos_dirent_t;

typedef struct vos_stat {
    unsigned char is_dir;
    unsigned char _pad[3];
    unsigned int size;
    unsigned short wtime;
    unsigned short wdate;
} vos_stat_t;

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

static int sys_stat_raw(const char* path, vos_stat_t* out) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_STAT), "b"(path), "c"(out)
        : "memory"
    );
    return ret;
}

static void print_errno(const char* what) {
    if (!what) what = "error";
    printf("\x1b[31;1m%s\x1b[0m: %s\n", what, strerror(errno));
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
    puts("\x1b[36;1mBuilt-ins:\x1b[0m");
    puts("  \x1b[33;1mhelp\x1b[0m               Show this help");
    puts("  \x1b[33;1mexit\x1b[0m               Exit the shell");
    puts("  \x1b[33;1mcd\x1b[0m [dir]            Change directory");
    puts("  \x1b[33;1mpwd\x1b[0m                Print current directory");
    puts("  \x1b[33;1mls\x1b[0m [opts] [path]    List directory contents");
    puts("  \x1b[33;1mcat\x1b[0m <file>          Print a file");
    puts("  \x1b[33;1mclear\x1b[0m              Clear the screen");

    puts("");
    puts("\x1b[36;1mPrograms in /bin:\x1b[0m");

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

typedef struct ls_entry {
    char name[64];
    unsigned char is_dir;
    unsigned char is_exec;
    unsigned short wtime;
    unsigned short wdate;
    unsigned int size;
} ls_entry_t;

typedef struct ls_opts {
    int show_all;
    int long_format;
    int bytes;
    int human;
} ls_opts_t;

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c + ('a' - 'A'));
    return c;
}

static int str_eq_ci(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (ascii_lower(*a) != ascii_lower(*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

static int has_ext_ci(const char* name, const char* ext) {
    if (!name || !ext) return 0;
    const char* dot = strrchr(name, '.');
    if (!dot || dot == name) return 0;
    return str_eq_ci(dot, ext);
}

static int ls_entry_cmp(const ls_entry_t* a, const ls_entry_t* b) {
    if (!a || !b) return 0;
    if (a->is_dir != b->is_dir) {
        return a->is_dir ? -1 : 1;
    }
    return strcmp(a->name, b->name);
}

static void ls_sort(ls_entry_t* entries, int count) {
    for (int i = 1; i < count; i++) {
        ls_entry_t key = entries[i];
        int j = i;
        while (j > 0 && ls_entry_cmp(&entries[j - 1], &key) > 0) {
            entries[j] = entries[j - 1];
            j--;
        }
        entries[j] = key;
    }
}

static void ls_format_size(char* out, size_t cap, unsigned int bytes, const ls_opts_t* opts) {
    if (!out || cap == 0) return;
    out[0] = '\0';

    if (opts && opts->bytes) {
        snprintf(out, cap, "%u", bytes);
        return;
    }

    if (opts && opts->human) {
        const char* suf = "B";
        unsigned int v = bytes;
        if (v >= 1024u * 1024u * 1024u) {
            v /= 1024u * 1024u * 1024u;
            suf = "G";
        } else if (v >= 1024u * 1024u) {
            v /= 1024u * 1024u;
            suf = "M";
        } else if (v >= 1024u) {
            v /= 1024u;
            suf = "K";
        }
        snprintf(out, cap, "%u%s", v, suf);
        return;
    }

    unsigned int kib = (bytes == 0) ? 0u : (unsigned int)((bytes + 1023u) / 1024u);
    snprintf(out, cap, "%uK", kib);
}

static void ls_print_mtime(unsigned short wdate, unsigned short wtime) {
    if (wdate == 0) {
        fputs("????", stdout);
        fputc('-', stdout);
        fputs("??", stdout);
        fputc('-', stdout);
        fputs("??", stdout);
        fputc(' ', stdout);
        fputs("??", stdout);
        fputc(':', stdout);
        fputs("??", stdout);
        return;
    }

    unsigned int year = 1980u + (unsigned int)((wdate >> 9) & 0x7Fu);
    unsigned int month = (unsigned int)((wdate >> 5) & 0x0Fu);
    unsigned int day = (unsigned int)(wdate & 0x1Fu);
    unsigned int hour = (unsigned int)((wtime >> 11) & 0x1Fu);
    unsigned int minute = (unsigned int)((wtime >> 5) & 0x3Fu);

    printf("%04u-%02u-%02u %02u:%02u", year, month, day, hour, minute);
}

static const char* ls_color_for(const ls_entry_t* e) {
    if (!e) return "";

    // Theme assumes default is white-on-blue; avoid dark/blue-ish foregrounds.
    if (e->is_dir) return "\x1b[36;1m";     // cyan + bold
    if (e->is_exec) return "\x1b[32;1m";    // green + bold

    const char* name = e->name;
    if (name[0] == '.') return "\x1b[37m";  // light grey for dotfiles

    if (strcmp(name, "Makefile") == 0 || strcmp(name, "makefile") == 0) return "\x1b[33;1m";

    if (has_ext_ci(name, ".c") || has_ext_ci(name, ".h") || has_ext_ci(name, ".cpp") || has_ext_ci(name, ".hpp") ||
        has_ext_ci(name, ".cc") || has_ext_ci(name, ".s") || has_ext_ci(name, ".asm") || has_ext_ci(name, ".ld") ||
        has_ext_ci(name, ".mk")) {
        return "\x1b[33;1m"; // yellow + bold (via 33 then 1)
    }

    if (has_ext_ci(name, ".bas") || has_ext_ci(name, ".lua") || has_ext_ci(name, ".sh") || has_ext_ci(name, ".py")) {
        return "\x1b[35;1m"; // magenta + bold
    }

    if (has_ext_ci(name, ".txt") || has_ext_ci(name, ".md") || has_ext_ci(name, ".cfg") || has_ext_ci(name, ".ini")) {
        return "\x1b[37;1m"; // bright white
    }

    if (has_ext_ci(name, ".img") || has_ext_ci(name, ".iso") || has_ext_ci(name, ".tar") || has_ext_ci(name, ".gz") ||
        has_ext_ci(name, ".zip")) {
        return "\x1b[31;1m"; // red + bold
    }

    return "";
}

static void ls_print_name(const ls_entry_t* e) {
    const char* CLR_RESET = "\x1b[0m";
    const char* seq = ls_color_for(e);
    if (seq[0]) fputs(seq, stdout);
    fputs(e->name, stdout);
    if (e->is_dir) {
        fputc('/', stdout);
    } else if (e->is_exec) {
        fputc('*', stdout);
    }
    if (seq[0]) fputs(CLR_RESET, stdout);
}

static void cmd_ls(int argc, char** argv) {
    ls_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.long_format = 1; // default: useful info

    const char* paths[8];
    int path_count = 0;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (!a || a[0] == '\0') continue;
        if (a[0] != '-' || a[1] == '\0') {
            if (path_count < (int)(sizeof(paths) / sizeof(paths[0]))) {
                paths[path_count++] = a;
            }
            continue;
        }
        if (strcmp(a, "--") == 0) {
            for (int j = i + 1; j < argc; j++) {
                if (path_count < (int)(sizeof(paths) / sizeof(paths[0]))) {
                    paths[path_count++] = argv[j];
                }
            }
            break;
        }
        if (strcmp(a, "--help") == 0) {
            puts("Usage: ls [options] [path]");
            puts("Options:");
            puts("  -l   long format (default)");
            puts("  -1   names only");
            puts("  -a   show hidden entries");
            puts("  -b   sizes in bytes");
            puts("  -h   human-readable sizes");
            return;
        }
        for (const char* p = a + 1; *p; p++) {
            switch (*p) {
                case 'l': opts.long_format = 1; break;
                case '1': opts.long_format = 0; break;
                case 'a': opts.show_all = 1; break;
                case 'b': opts.bytes = 1; opts.human = 0; break;
                case 'h': opts.human = 1; opts.bytes = 0; break;
                default:
                    printf("ls: unknown option -%c (try --help)\n", *p);
                    return;
            }
        }
    }

    if (path_count == 0) {
        paths[path_count++] = ".";
    }

    for (int pi = 0; pi < path_count; pi++) {
        const char* path = paths[pi];
        if (!path || path[0] == '\0') {
            path = ".";
        }

        vos_stat_t st;
        int st_rc = sys_stat_raw(path, &st);
        if (st_rc < 0) {
            errno = -st_rc;
            print_errno("ls");
            continue;
        }

        if (!st.is_dir) {
            ls_entry_t e;
            memset(&e, 0, sizeof(e));
            strncpy(e.name, path, sizeof(e.name) - 1u);
            e.name[sizeof(e.name) - 1u] = '\0';
            e.is_dir = 0;
            e.size = st.size;
            e.wtime = st.wtime;
            e.wdate = st.wdate;
            e.is_exec = (e.size >= 4u && is_elf_file(".", path)) ? 1u : 0u;
            if (opts.long_format) {
                char sbuf[16];
                ls_format_size(sbuf, sizeof(sbuf), e.size, &opts);
                const char* tclr = e.is_exec ? "\x1b[32;1m" : "\x1b[37m";
                printf("%s%c\x1b[0m ", tclr, e.is_exec ? 'x' : '-');
                fputs("\x1b[37m", stdout);
                ls_print_mtime(e.wdate, e.wtime);
                fputs("\x1b[0m  ", stdout);
                fputs("\x1b[33;1m", stdout);
                printf("%6s", sbuf);
                fputs("\x1b[0m ", stdout);
            }
            ls_print_name(&e);
            putchar('\n');
            continue;
        }

        int fd = open(path, O_RDONLY | O_DIRECTORY);
        if (fd < 0) {
            print_errno("ls");
            continue;
        }

        ls_entry_t* entries = (ls_entry_t*)malloc(sizeof(ls_entry_t) * 256u);
        if (!entries) {
            close(fd);
            puts("ls: out of memory");
            continue;
        }

        int entry_count = 0;
        if (opts.show_all) {
            ls_entry_t* dot = &entries[entry_count++];
            memset(dot, 0, sizeof(*dot));
            strcpy(dot->name, ".");
            dot->is_dir = 1;
            dot->size = 0;
            dot->wtime = 0;
            dot->wdate = 0;

            if (entry_count < 256) {
                ls_entry_t* dotdot = &entries[entry_count++];
                memset(dotdot, 0, sizeof(*dotdot));
                strcpy(dotdot->name, "..");
                dotdot->is_dir = 1;
                dotdot->size = 0;
                dotdot->wtime = 0;
                dotdot->wdate = 0;
            }
        }

        vos_dirent_t de;
        while (entry_count < 256 && sys_readdir(fd, &de) > 0) {
            if (de.name[0] == '\0') continue;
            if (!opts.show_all && de.name[0] == '.') continue;

            ls_entry_t* e = &entries[entry_count++];
            memset(e, 0, sizeof(*e));
            strncpy(e->name, de.name, sizeof(e->name) - 1u);
            e->name[sizeof(e->name) - 1u] = '\0';
            e->is_dir = de.is_dir ? 1u : 0u;
            e->size = de.size;
            e->wtime = de.wtime;
            e->wdate = de.wdate;
            e->is_exec = (!e->is_dir && e->size >= 4u && is_elf_file(path, e->name)) ? 1u : 0u;
        }
        close(fd);

        ls_sort(entries, entry_count);

        size_t sizew = 0;
        if (opts.long_format) {
            for (int i = 0; i < entry_count; i++) {
                char sbuf[16];
                ls_format_size(sbuf, sizeof(sbuf), entries[i].size, &opts);
                size_t n = strlen(sbuf);
                if (n > sizew) sizew = n;
            }
            if (sizew < 1) sizew = 1;
            if (sizew > 12) sizew = 12;
        }

        if (path_count > 1) {
            if (pi > 0) putchar('\n');
            printf("\x1b[36;1m%s:\x1b[0m\n", path);
        }

        for (int i = 0; i < entry_count; i++) {
            ls_entry_t* e = &entries[i];
            if (opts.long_format) {
                char sbuf[16];
                ls_format_size(sbuf, sizeof(sbuf), e->size, &opts);

                char t = e->is_dir ? 'd' : (e->is_exec ? 'x' : '-');
                const char* tclr = e->is_dir ? "\x1b[36;1m" : (e->is_exec ? "\x1b[32;1m" : "\x1b[0m");
                printf("%s%c\x1b[0m ", tclr, t);
                fputs("\x1b[37m", stdout);
                ls_print_mtime(e->wdate, e->wtime);
                fputs("\x1b[0m  ", stdout);
                fputs("\x1b[33;1m", stdout);
                printf("%*s", (int)sizew, sbuf);
                fputs("\x1b[0m ", stdout);
            }

            ls_print_name(e);
            putchar('\n');
        }

        free(entries);
    }
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

    puts("\x1b[36;1mVOS user shell\x1b[0m (linenoise). Type '\x1b[33;1mhelp\x1b[0m' for help.");

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
