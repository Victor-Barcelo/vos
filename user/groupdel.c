// groupdel - delete a group
// Usage: groupdel groupname

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int disk_available(void) {
    struct stat st;
    return (stat("/disk/etc", &st) == 0);
}

// Remove a line from a file that starts with "groupname:"
static int remove_group_line(const char* path, const char* groupname) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;

    char tmppath[256];
    snprintf(tmppath, sizeof(tmppath), "%s.tmp", path);

    int fd = open(tmppath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fclose(f);
        return -1;
    }

    size_t glen = strlen(groupname);
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, groupname, glen) == 0 && line[glen] == ':') {
            continue;
        }
        (void)write(fd, line, strlen(line));
    }

    fclose(f);
    close(fd);

    (void)unlink(path);
    (void)rename(tmppath, path);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: groupdel groupname\n");
        return 1;
    }

    const char* groupname = argv[1];

    // Only root can delete groups
    if (getuid() != 0) {
        fprintf(stderr, "groupdel: permission denied (must be root)\n");
        return 1;
    }

    // Check if group exists
    struct group* gr = getgrnam(groupname);
    if (!gr) {
        fprintf(stderr, "groupdel: group '%s' does not exist\n", groupname);
        return 1;
    }

    // Don't allow deleting root group
    if (gr->gr_gid == 0) {
        fprintf(stderr, "groupdel: cannot delete root group\n");
        return 1;
    }

    if (remove_group_line("/etc/group", groupname) < 0) {
        fprintf(stderr, "groupdel: failed to update /etc/group\n");
        return 1;
    }

    if (disk_available()) {
        remove_group_line("/disk/etc/group", groupname);
    }

    printf("Group '%s' deleted\n", groupname);
    return 0;
}
