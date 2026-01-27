// userdel - delete a user
// Usage: userdel [-r] username
//   -r  Remove home directory

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int disk_available(void) {
    struct stat st;
    return (stat("/disk/etc", &st) == 0);
}

// Remove a line from a file that starts with "username:"
static int remove_user_line(const char* path, const char* username) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;

    char tmppath[256];
    snprintf(tmppath, sizeof(tmppath), "%s.tmp", path);

    int fd = open(tmppath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fclose(f);
        return -1;
    }

    size_t ulen = strlen(username);
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // Skip lines that match "username:"
        if (strncmp(line, username, ulen) == 0 && line[ulen] == ':') {
            continue;
        }
        (void)write(fd, line, strlen(line));
    }

    fclose(f);
    close(fd);

    // Replace original with temp
    (void)unlink(path);
    (void)rename(tmppath, path);
    return 0;
}

// Recursively remove a directory (simple version)
static void rmdir_r(const char* path) {
    // For simplicity, just try rmdir (works if empty)
    // In a full implementation, we'd need to recurse
    (void)rmdir(path);
}

static void usage(void) {
    fprintf(stderr, "Usage: userdel [-r] username\n");
    fprintf(stderr, "  -r  Remove home directory\n");
    exit(1);
}

int main(int argc, char* argv[]) {
    int remove_home = 0;
    char* username = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            remove_home = 1;
        } else if (argv[i][0] != '-') {
            username = argv[i];
        } else {
            usage();
        }
    }

    if (!username) {
        usage();
    }

    // Only root can delete users
    if (getuid() != 0) {
        fprintf(stderr, "userdel: permission denied (must be root)\n");
        return 1;
    }

    // Check if user exists
    struct passwd* pw = getpwnam(username);
    if (!pw) {
        fprintf(stderr, "userdel: user '%s' does not exist\n", username);
        return 1;
    }

    // Don't allow deleting root
    if (pw->pw_uid == 0) {
        fprintf(stderr, "userdel: cannot delete root user\n");
        return 1;
    }

    char homedir[128];
    if (pw->pw_dir) {
        strncpy(homedir, pw->pw_dir, sizeof(homedir) - 1);
        homedir[sizeof(homedir) - 1] = '\0';
    } else {
        snprintf(homedir, sizeof(homedir), "/home/%s", username);
    }

    // Remove from /etc/passwd
    if (remove_user_line("/etc/passwd", username) < 0) {
        fprintf(stderr, "userdel: failed to update /etc/passwd\n");
        return 1;
    }

    // Remove from /etc/group (the user's primary group)
    (void)remove_user_line("/etc/group", username);

    // Update persistent storage
    if (disk_available()) {
        remove_user_line("/disk/etc/passwd", username);
        remove_user_line("/disk/etc/group", username);
    }

    // Remove home directory if requested
    if (remove_home) {
        rmdir_r(homedir);
        if (disk_available()) {
            char disk_home[128];
            snprintf(disk_home, sizeof(disk_home), "/disk/home/%s", username);
            rmdir_r(disk_home);
        }
    }

    printf("User '%s' deleted\n", username);
    return 0;
}
