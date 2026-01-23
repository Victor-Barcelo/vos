#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
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

static char g_username[32] = "user";
static char g_home[128] = "/";

static void trim_newline(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1u] == '\n' || s[n - 1u] == '\r')) {
        s[n - 1u] = '\0';
        n--;
    }
}

static int parse_u32(const char* s, unsigned int* out) {
    if (!out) return -1;
    *out = 0;
    if (!s || *s == '\0') return -1;
    char* end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (!end || end == s) return -1;
    *out = (unsigned int)v;
    return 0;
}

static void resolve_user_identity(void) {
    uid_t uid = getuid();
    if (uid == 0) {
        strncpy(g_username, "root", sizeof(g_username) - 1u);
        g_username[sizeof(g_username) - 1u] = '\0';
        strncpy(g_home, "/home/root", sizeof(g_home) - 1u);
        g_home[sizeof(g_home) - 1u] = '\0';
    }

    FILE* f = fopen("/etc/passwd", "r");
    if (!f) {
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);

        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;

        // name:pass:uid:gid:home:shell
        char* fields[6] = {0};
        int nf = 0;
        char* q = p;
        for (; nf < 6; nf++) {
            fields[nf] = q;
            char* c = strchr(q, ':');
            if (!c) break;
            *c = '\0';
            q = c + 1;
        }

        if (!fields[0] || fields[0][0] == '\0') continue;
        if (!fields[2] || fields[2][0] == '\0') continue;

        unsigned int file_uid = 0;
        if (parse_u32(fields[2], &file_uid) != 0) continue;
        if ((uid_t)file_uid != uid) continue;

        strncpy(g_username, fields[0], sizeof(g_username) - 1u);
        g_username[sizeof(g_username) - 1u] = '\0';

        if (fields[4] && fields[4][0] != '\0') {
            strncpy(g_home, fields[4], sizeof(g_home) - 1u);
            g_home[sizeof(g_home) - 1u] = '\0';
        } else {
            snprintf(g_home, sizeof(g_home), "/home/%s", g_username);
        }
        break;
    }

    fclose(f);
}

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

    puts("");
    puts("\x1b[36;1mPrograms in /usr/bin:\x1b[0m");

    fd = open("/usr/bin", O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        // /usr is optional (disk-backed).
        return;
    }

    count = 0;
    while (count < (int)(sizeof(names) / sizeof(names[0])) && sys_readdir(fd, &de) > 0) {
        if (de.name[0] == '\0') continue;
        if (de.is_dir) continue;
        strncpy(names[count], de.name, sizeof(names[count]) - 1u);
        names[count][sizeof(names[count]) - 1u] = '\0';
        count++;
    }
    close(fd);

    for (int i = 1; i < count; i++) {
        char tmp2[64];
        strncpy(tmp2, names[i], sizeof(tmp2));
        tmp2[sizeof(tmp2) - 1u] = '\0';

        int j = i;
        while (j > 0 && strcmp(names[j - 1], tmp2) > 0) {
            strncpy(names[j], names[j - 1], sizeof(names[j]));
            names[j][sizeof(names[j]) - 1u] = '\0';
            j--;
        }
        strncpy(names[j], tmp2, sizeof(names[j]));
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
    const char* dir = (argc >= 2) ? argv[1] : g_home;
    char tmp[256];
    if (dir && dir[0] == '~') {
        if (dir[1] == '\0') {
            dir = g_home;
        } else if (dir[1] == '/') {
            snprintf(tmp, sizeof(tmp), "%s%s", g_home, dir + 1);
            dir = tmp;
        }
    }
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

static void add_prefixed_completion(linenoiseCompletions* lc, const char* before, const char* completion) {
    if (!lc || !before || !completion) {
        return;
    }
    char tmp[512];
    int n = snprintf(tmp, sizeof(tmp), "%s%s", before, completion);
    if (n <= 0 || (size_t)n >= sizeof(tmp)) {
        return;
    }
    linenoiseAddCompletion(lc, tmp);
}

static void complete_command_token(const char* before, const char* word, linenoiseCompletions* lc) {
    static const char* builtins[] = {"help", "exit", "cd", "pwd", "ls", "cat", "clear"};

    size_t n = strlen(word);
    for (unsigned int i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        const char* b = builtins[i];
        if (strncmp(b, word, n) == 0) {
            add_prefixed_completion(lc, before, b);
        }
    }

    // Complete executables from /bin and /usr/bin.
    const char* prefix = word;
    const char* base = "";
    const char* dir = "/bin";
    char tmp[128];
    if (strncmp(word, "/bin/", 5) == 0) {
        prefix = word + 5;
        base = "/bin/";
        dir = "/bin";
    } else if (strncmp(word, "/usr/bin/", 9) == 0) {
        prefix = word + 9;
        base = "/usr/bin/";
        dir = "/usr/bin";
    }

    int fd = open(dir, O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        vos_dirent_t de;
        while (sys_readdir(fd, &de) > 0) {
            if (de.is_dir) continue;
            if (de.name[0] == '\0') continue;
            if (strncmp(de.name, prefix, strlen(prefix)) != 0) continue;
            snprintf(tmp, sizeof(tmp), "%s%s", base, de.name);
            add_prefixed_completion(lc, before, tmp);
        }
        close(fd);
    }

    // If completing a bare word, also look in /usr/bin.
    if (base[0] == '\0') {
        fd = open("/usr/bin", O_RDONLY | O_DIRECTORY);
        if (fd >= 0) {
            vos_dirent_t de;
            while (sys_readdir(fd, &de) > 0) {
                if (de.is_dir) continue;
                if (de.name[0] == '\0') continue;
                if (strncmp(de.name, prefix, strlen(prefix)) != 0) continue;
                add_prefixed_completion(lc, before, de.name);
            }
            close(fd);
        }
    }
}

static void complete_path_token(const char* before, const char* tok, linenoiseCompletions* lc, int dirs_only) {
    if (!before || !tok || !lc) {
        return;
    }

    const char* prefix = tok;
    char dir_path[256];
    char base_path[256];
    dir_path[0] = '\0';
    base_path[0] = '\0';

    const char* slash = strrchr(tok, '/');
    if (slash) {
        size_t dlen = (size_t)(slash - tok);
        if (dlen == 0) {
            strcpy(dir_path, "/");
            strcpy(base_path, "/");
        } else {
            if (dlen >= sizeof(dir_path)) {
                return;
            }
            memcpy(dir_path, tok, dlen);
            dir_path[dlen] = '\0';
            snprintf(base_path, sizeof(base_path), "%s/", dir_path);
        }
        prefix = slash + 1;
    } else {
        strcpy(dir_path, ".");
        base_path[0] = '\0';
        prefix = tok;
    }

    int fd = open(dir_path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return;
    }

    vos_dirent_t de;
    while (sys_readdir(fd, &de) > 0) {
        if (de.name[0] == '\0') continue;
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
            // still allow, but only when explicitly requested
            if (prefix[0] != '.') continue;
        }
        if (strncmp(de.name, prefix, strlen(prefix)) != 0) continue;
        if (dirs_only && !de.is_dir) continue;

        char cand[512];
        if (de.is_dir) {
            snprintf(cand, sizeof(cand), "%s%s%s/", before, base_path, de.name);
        } else {
            snprintf(cand, sizeof(cand), "%s%s%s", before, base_path, de.name);
        }
        linenoiseAddCompletion(lc, cand);
    }

    close(fd);
}

static void completion_cb(const char* buf, linenoiseCompletions* lc) {
    if (!buf || !lc) {
        return;
    }

    size_t len = strlen(buf);
    const char* end = buf + len;

    const char* tok = end;
    while (tok > buf && tok[-1] != ' ' && tok[-1] != '\t') {
        tok--;
    }

    char before[512];
    size_t before_len = (size_t)(tok - buf);
    if (before_len >= sizeof(before)) {
        return;
    }
    memcpy(before, buf, before_len);
    before[before_len] = '\0';

    int word_index = 0;
    bool in_word = false;
    for (const char* p = buf; p < tok; p++) {
        if (*p == ' ' || *p == '\t') {
            in_word = false;
            continue;
        }
        if (!in_word) {
            in_word = true;
            word_index++;
        }
    }

    // Extract the first word (command) to specialize path completion.
    char cmd[64];
    cmd[0] = '\0';
    const char* p = buf;
    while (*p == ' ' || *p == '\t') p++;
    size_t ci = 0;
    while (*p && *p != ' ' && *p != '\t' && ci + 1 < sizeof(cmd)) {
        cmd[ci++] = *p++;
    }
    cmd[ci] = '\0';

    if (word_index == 0) {
        complete_command_token(before, tok, lc);
        return;
    }

    int dirs_only = (strcmp(cmd, "cd") == 0);
    complete_path_token(before, tok, lc, dirs_only);
}

static int run_external(int argc, char** argv, bool print_exit) {
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
    if (pid < 0 && strchr(cmd, '/') == NULL) {
        snprintf(pathbuf, sizeof(pathbuf), "/usr/bin/%s", cmd);
        exec_path = pathbuf;
        sargv[0] = exec_path;
        pid = sys_spawn(exec_path, sargv, (uint32_t)argc);
    }

    if (pid < 0) {
        errno = -pid;
        print_errno(cmd);
        return -1;
    }

    // Make the child the terminal foreground process so Ctrl+C can stop it.
    int fg = pid;
    (void)ioctl(0, TIOCSPGRP, &fg);

    int code = sys_wait((uint32_t)pid);

    // Restore "no foreground process" while at the prompt.
    int none = 0;
    (void)ioctl(0, TIOCSPGRP, &none);

    if (print_exit) {
        printf("exit %d\n", code);
    }
    return 0;
}

typedef struct sh_redir {
    const char* in_path;
    const char* out_path;
    const char* err_path;
    bool out_append;
    bool err_append;
} sh_redir_t;

static bool sh_parse_redir_token(const char* tok, sh_redir_t* r, const char** out_path, bool* out_append) {
    if (!tok || !r || !out_path || !out_append) {
        return false;
    }

    *out_path = NULL;
    *out_append = false;

    if (tok[0] == '\0') {
        return false;
    }

    if (tok[0] == '<') {
        *out_path = tok[1] ? (tok + 1) : NULL;
        return true;
    }

    if (tok[0] == '>') {
        if (tok[1] == '>') {
            *out_append = true;
            *out_path = tok[2] ? (tok + 2) : NULL;
        } else {
            *out_path = tok[1] ? (tok + 1) : NULL;
        }
        return true;
    }

    if (tok[0] == '2' && tok[1] == '>') {
        if (tok[2] == '>') {
            *out_append = true;
            *out_path = tok[3] ? (tok + 3) : NULL;
        } else {
            *out_path = tok[2] ? (tok + 2) : NULL;
        }
        return true;
    }

    return false;
}

static int sh_apply_redirections(const sh_redir_t* r, int saved[3]) {
    saved[0] = -1;
    saved[1] = -1;
    saved[2] = -1;

    if (!r) {
        return 0;
    }

    if (r->in_path) {
        saved[0] = dup(STDIN_FILENO);
        if (saved[0] < 0) {
            return -1;
        }
        int fd = open(r->in_path, O_RDONLY);
        if (fd < 0) {
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (r->out_path) {
        saved[1] = dup(STDOUT_FILENO);
        if (saved[1] < 0) {
            return -1;
        }
        int flags = O_WRONLY | O_CREAT | (r->out_append ? O_APPEND : O_TRUNC);
        int fd = open(r->out_path, flags, 0666);
        if (fd < 0) {
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (r->err_path) {
        saved[2] = dup(STDERR_FILENO);
        if (saved[2] < 0) {
            return -1;
        }
        int flags = O_WRONLY | O_CREAT | (r->err_append ? O_APPEND : O_TRUNC);
        int fd = open(r->err_path, flags, 0666);
        if (fd < 0) {
            return -1;
        }
        if (dup2(fd, STDERR_FILENO) < 0) {
            close(fd);
            return -1;
        }
        close(fd);
    }

    return 0;
}

static void sh_restore_redirections(const int saved[3]) {
    if (saved[0] >= 0) {
        (void)dup2(saved[0], STDIN_FILENO);
        close(saved[0]);
    }
    if (saved[1] >= 0) {
        (void)dup2(saved[1], STDOUT_FILENO);
        close(saved[1]);
    }
    if (saved[2] >= 0) {
        (void)dup2(saved[2], STDERR_FILENO);
        close(saved[2]);
    }
}

static int sh_execute_argv(int argc, char** argv, bool print_exit) {
    if (argc <= 0) {
        return 0;
    }

    if (strcmp(argv[0], "exit") == 0) {
        return 1;
    }
    if (strcmp(argv[0], "help") == 0) {
        cmd_help();
        return 0;
    }
    if (strcmp(argv[0], "cd") == 0) {
        cmd_cd(argc, argv);
        return 0;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        cmd_pwd();
        return 0;
    }
    if (strcmp(argv[0], "ls") == 0) {
        cmd_ls(argc, argv);
        return 0;
    }
    if (strcmp(argv[0], "cat") == 0) {
        cmd_cat(argc, argv);
        return 0;
    }
    if (strcmp(argv[0], "clear") == 0) {
        cmd_clear();
        return 0;
    }

    (void)run_external(argc, argv, print_exit);
    return 0;
}

static int sh_execute_line(char* line, bool print_exit) {
    if (!line) {
        return 0;
    }

    char* av_raw[SHELL_MAX_ARGS];
    int ac_raw = split_args(line, av_raw, SHELL_MAX_ARGS);
    if (ac_raw <= 0) {
        return 0;
    }

    sh_redir_t redir = {0};
    char* av[SHELL_MAX_ARGS];
    int ac = 0;

    for (int i = 0; i < ac_raw; i++) {
        char* tok = av_raw[i];
        if (!tok || tok[0] == '\0') {
            continue;
        }

        if (strcmp(tok, "(") == 0 || strcmp(tok, ")") == 0) {
            continue;
        }

        const char* path = NULL;
        bool append = false;
        if (sh_parse_redir_token(tok, &redir, &path, &append)) {
            if (tok[0] == '<') {
                if (!path) {
                    if (i + 1 < ac_raw) {
                        redir.in_path = av_raw[++i];
                    }
                } else {
                    redir.in_path = path;
                }
            } else if (tok[0] == '2') {
                if (!path) {
                    if (i + 1 < ac_raw) {
                        redir.err_path = av_raw[++i];
                    }
                } else {
                    redir.err_path = path;
                }
                redir.err_append = append;
            } else {
                if (!path) {
                    if (i + 1 < ac_raw) {
                        redir.out_path = av_raw[++i];
                    }
                } else {
                    redir.out_path = path;
                }
                redir.out_append = append;
            }
            continue;
        }

        if (ac < SHELL_MAX_ARGS) {
            av[ac++] = tok;
        }
    }

    if (ac <= 0) {
        return 0;
    }

    int saved[3];
    if (sh_apply_redirections(&redir, saved) != 0) {
        print_errno("redirect");
        sh_restore_redirections(saved);
        return 0;
    }

    int rc = sh_execute_argv(ac, av, print_exit);
    sh_restore_redirections(saved);
    return rc;
}

int main(int argc, char** argv) {
    linenoiseSetCompletionCallback(completion_cb);
    linenoiseHistorySetMaxLen(128);

    resolve_user_identity();

    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        char buf[SHELL_MAX_LINE];
        strncpy(buf, argv[2], sizeof(buf) - 1u);
        buf[sizeof(buf) - 1u] = '\0';
        return sh_execute_line(buf, false) ? 1 : 0;
    }

    puts("\x1b[36;1mVOS user shell\x1b[0m (linenoise). Type '\x1b[33;1mhelp\x1b[0m' for help.");

    char cwd[256];
    for (;;) {
        if (!getcwd(cwd, sizeof(cwd))) {
            strcpy(cwd, "/");
        }

        char display_cwd[256];
        const char* shown = cwd;
        if (g_home[0] == '/' && g_home[1] != '\0') {
            size_t hl = strlen(g_home);
            if (strncmp(cwd, g_home, hl) == 0 && (cwd[hl] == '\0' || cwd[hl] == '/')) {
                snprintf(display_cwd, sizeof(display_cwd), "~%s", cwd + hl);
                shown = display_cwd;
            }
        }

        char prompt[320];
        snprintf(prompt, sizeof(prompt), "%s@vos:%s$ ", g_username, shown);

        // Enable terminal mouse reporting while we are inside linenoise so clicks
        // can move the cursor and the wheel can navigate history.
        fputs("\x1b[?1000h\x1b[?1006h", stdout);
        fflush(stdout);
        char* line = linenoise(prompt);
        fputs("\x1b[?1000l\x1b[?1006l", stdout);
        fflush(stdout);
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
        } else {
            (void)sh_execute_argv(ac, av, true);
        }

        free(line);
    }

    return 0;
}
