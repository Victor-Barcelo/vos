#include "vfs.h"

#include "ctype.h"
#include "kerrno.h"
#include "kheap.h"
#include "minixfs.h"
#include "paging.h"
#include "pmm.h"
#include "ramfs.h"
#include "string.h"

// Convert Unix timestamp to FAT date/time format
static void unix_to_fat_ts(uint32_t unix_ts, uint16_t* out_wtime, uint16_t* out_wdate) {
    // Days per month (non-leap year)
    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};

    // Calculate date/time from unix timestamp
    uint32_t secs = unix_ts;
    uint32_t days = secs / 86400;
    secs %= 86400;

    int hour = (int)(secs / 3600);
    secs %= 3600;
    int minute = (int)(secs / 60);
    int second = (int)(secs % 60);

    // Calculate year (starting from 1970)
    int year = 1970;
    while (1) {
        int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        int yday = leap ? 366 : 365;
        if (days < (uint32_t)yday) break;
        days -= yday;
        year++;
    }

    // Calculate month and day
    int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    int month = 0;
    while (month < 12) {
        int mday = mdays[month];
        if (month == 1 && leap) mday++;
        if (days < (uint32_t)mday) break;
        days -= mday;
        month++;
    }
    int day = (int)days + 1;
    month++;

    // FAT date: bits 0-4 = day, 5-8 = month, 9-15 = year since 1980
    int fat_year = year - 1980;
    if (fat_year < 0) fat_year = 0;
    if (fat_year > 127) fat_year = 127;
    *out_wdate = (uint16_t)((fat_year << 9) | (month << 5) | day);

    // FAT time: bits 0-4 = second/2, 5-10 = minute, 11-15 = hour
    *out_wtime = (uint16_t)((hour << 11) | (minute << 5) | (second / 2));
}

// Keep these in sync with newlib's <sys/_default_fcntl.h>.
enum {
    VFS_O_RDONLY    = 0,
    VFS_O_WRONLY    = 1,
    VFS_O_RDWR      = 2,
    VFS_O_ACCMODE   = 3,
    VFS_O_APPEND    = 0x0008,
    VFS_O_CREAT     = 0x0200,
    VFS_O_TRUNC     = 0x0400,
    VFS_O_EXCL      = 0x0800,
    VFS_O_DIRECTORY = 0x200000,
};

enum {
    VFS_SEEK_SET = 0,
    VFS_SEEK_CUR = 1,
    VFS_SEEK_END = 2,
};

typedef enum {
    VFS_BACKEND_INITRAMFS = 0,
    VFS_BACKEND_RAMFS = 1,
    VFS_BACKEND_MINIXFS = 2,
} vfs_backend_t;

typedef enum {
    VFS_HANDLE_FILE = 0,
    VFS_HANDLE_DIR = 1,
} vfs_handle_kind_t;

struct vfs_handle {
    vfs_handle_kind_t kind;
    vfs_backend_t backend;
    uint32_t flags;
    uint32_t off;
    uint32_t refcount;
    char abs_path[VFS_PATH_MAX];

    // File state.
    const uint8_t* ro_data; // not owned (initramfs/rom)
    uint8_t* buf;           // owned (copy-on-write / writable backends)
    uint32_t size;
    uint32_t cap;
    bool dirty;

    // Directory state.
    vfs_dirent_t* ents; // owned
    uint32_t ent_count;
    uint32_t ent_index;
};

static void abs_dirname(const char* abs, char out[VFS_PATH_MAX]);

static bool ci_eq(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }
    for (;;) {
        char ca = *a++;
        char cb = *b++;
        if (tolower((unsigned char)ca) != tolower((unsigned char)cb)) {
            return false;
        }
        if (ca == '\0') {
            return true;
        }
    }
}

static bool ci_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) {
        return false;
    }
    while (*prefix) {
        char cs = *s++;
        char cp = *prefix++;
        if (tolower((unsigned char)cs) != tolower((unsigned char)cp)) {
            return false;
        }
    }
    return true;
}

static bool ci_eq_n(const char* a, const char* b, uint32_t n) {
    if (!a || !b) {
        return false;
    }
    for (uint32_t i = 0; i < n; i++) {
        char ca = a[i];
        char cb = b[i];
        if (tolower((unsigned char)ca) != tolower((unsigned char)cb)) {
            return false;
        }
        if (ca == '\0') {
            return cb == '\0';
        }
    }
    return true;
}

static bool abs_is_mount(const char* abs, const char* mount) {
    if (!abs || !mount) {
        return false;
    }
    uint32_t mlen = (uint32_t)strlen(mount);
    if (mlen == 0) {
        return false;
    }
    if (!ci_eq_n(abs, mount, mlen)) {
        return false;
    }
    char next = abs[mlen];
    return next == '\0' || next == '/';
}

static bool abs_alias_to(const char* abs, const char* mount, const char* target, char out[VFS_PATH_MAX]) {
    if (!out) {
        return false;
    }
    out[0] = '\0';
    if (!abs || abs[0] != '/') {
        return false;
    }
    if (!abs_is_mount(abs, mount)) {
        return false;
    }

    size_t mlen = strlen(mount);
    strncpy(out, target, VFS_PATH_MAX - 1u);
    out[VFS_PATH_MAX - 1u] = '\0';
    if (ci_eq(abs, mount)) {
        return true;
    }

    size_t used = strlen(out);
    if (used < VFS_PATH_MAX - 1u) {
        strncat(out, abs + mlen, (VFS_PATH_MAX - 1u) - used);
    }
    return true;
}

// Check if a path exists (for overlay fallback logic).
// Works with both /ram/... (ramfs) and /disk/... (minixfs) paths.
static bool vfs_path_exists_raw(const char* path) {
    if (!path) return false;
    // Check minixfs for /disk/... paths
    if (ci_starts_with(path, "/disk") && (path[5] == '/' || path[5] == '\0')) {
        if (!minixfs_is_ready()) return false;
        const char* rel = path + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";
        minixfs_stat_t mst;
        return minixfs_stat(rel, &mst);
    }
    // Check ramfs for /ram/... paths
    bool is_dir;
    while (*path == '/') path++;
    return ramfs_stat_ex(path, &is_dir, NULL, NULL, NULL);
}

// Convenience aliases so userland can use Linux-like paths while we keep the
// actual mountpoints simple:
//   /usr  -> /disk/usr   (toolchains, headers, libs)
//   /etc  -> /disk/etc   (persistent config)
//   /home -> /disk/home  (user homes)
//   /var  -> /disk/var   (logs/state; optional)
//   /tmp  -> /ram/tmp    (ephemeral scratch)
//
// These aliases use overlay semantics: if the destination path doesn't exist
// in RAMFS, we fall back to the original path (which may resolve to initramfs).
// This allows initramfs defaults to be overridden by persistent storage.
static const char* abs_apply_posix_aliases(const char* abs, char tmp[VFS_PATH_MAX]) {
    if (!abs || !tmp) {
        return abs;
    }

    // /tmp -> /ram/tmp (always, ephemeral storage)
    if (abs_alias_to(abs, "/tmp", "/ram/tmp", tmp)) {
        return tmp;
    }

    // /etc -> /ram/etc (always; init populates from initramfs + /disk/etc overlay)
    if (abs_alias_to(abs, "/etc", "/ram/etc", tmp)) {
        return tmp;
    }

    // For persistent aliases: alias to disk if the specific path exists there,
    // or if the parent directory exists on disk (allows creating new files).
    // This allows initramfs defaults to be accessible at standard paths,
    // while disk files can override them when present.
    if (abs_alias_to(abs, "/usr", "/disk/usr", tmp)) {
        if (vfs_path_exists_raw(tmp)) return tmp;  // specific path exists on disk
        // Check if parent exists on disk (allows file creation)
        char parent[VFS_PATH_MAX];
        strncpy(parent, tmp, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        char* last_slash = strrchr(parent, '/');
        if (last_slash && last_slash != parent) {
            *last_slash = '\0';
            if (vfs_path_exists_raw(parent)) return tmp;
        }
    }
    if (abs_alias_to(abs, "/home", "/disk/home", tmp)) {
        if (vfs_path_exists_raw(tmp)) return tmp;
        char parent[VFS_PATH_MAX];
        strncpy(parent, tmp, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        char* last_slash = strrchr(parent, '/');
        if (last_slash && last_slash != parent) {
            *last_slash = '\0';
            if (vfs_path_exists_raw(parent)) return tmp;
        }
    }
    if (abs_alias_to(abs, "/var", "/disk/var", tmp)) {
        if (vfs_path_exists_raw(tmp)) return tmp;
        char parent[VFS_PATH_MAX];
        strncpy(parent, tmp, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        char* last_slash = strrchr(parent, '/');
        if (last_slash && last_slash != parent) {
            *last_slash = '\0';
            if (vfs_path_exists_raw(parent)) return tmp;
        }
    }
    if (abs_alias_to(abs, "/root", "/disk/root", tmp)) {
        if (vfs_path_exists_raw(tmp)) return tmp;
        char parent[VFS_PATH_MAX];
        strncpy(parent, tmp, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        char* last_slash = strrchr(parent, '/');
        if (last_slash && last_slash != parent) {
            *last_slash = '\0';
            if (vfs_path_exists_raw(parent)) return tmp;
        }
    }

    return abs;
}

static int32_t path_push(char* out, uint32_t cap, uint32_t* out_len, uint32_t* saved, uint32_t* depth, uint32_t saved_cap, const char* seg, uint32_t seg_len) {
    if (!out || !out_len || !saved || !depth || !seg) {
        return -EINVAL;
    }
    if (seg_len == 0) {
        return 0;
    }
    if (*depth >= saved_cap) {
        return -ENAMETOOLONG;
    }

    saved[*depth] = *out_len;
    (*depth)++;

    uint32_t need = seg_len + ((*out_len > 1u) ? 1u : 0u);
    if (*out_len + need + 1u > cap) {
        return -ENAMETOOLONG;
    }

    if (*out_len > 1u) {
        out[(*out_len)++] = '/';
    }
    for (uint32_t i = 0; i < seg_len; i++) {
        out[(*out_len)++] = seg[i];
    }
    out[*out_len] = '\0';
    return 0;
}

static void path_pop(char* out, uint32_t* out_len, uint32_t* depth, const uint32_t* saved) {
    if (!out || !out_len || !depth || !saved) {
        return;
    }
    if (*depth == 0) {
        *out_len = 1;
        out[1] = '\0';
        return;
    }
    (*depth)--;
    *out_len = saved[*depth];
    if (*out_len < 1u) {
        *out_len = 1;
    }
    out[*out_len] = '\0';
}

int32_t vfs_path_resolve(const char* cwd, const char* path, char out_abs[VFS_PATH_MAX]) {
    if (!out_abs) {
        return -EINVAL;
    }
    if (!path) {
        return -EINVAL;
    }
    if (!cwd || cwd[0] != '/') {
        cwd = "/";
    }

    out_abs[0] = '/';
    out_abs[1] = '\0';
    uint32_t out_len = 1;

    uint32_t saved[64];
    uint32_t depth = 0;

    bool is_abs = path[0] == '/';
    const char* p = NULL;

    // Seed with cwd segments if path is relative.
    if (!is_abs) {
        p = cwd;
        while (*p == '/') p++;
        while (*p) {
            while (*p == '/') p++;
            if (*p == '\0') break;
            const char* seg = p;
            uint32_t seg_len = 0;
            while (seg[seg_len] && seg[seg_len] != '/') seg_len++;
            p += seg_len;

            if (seg_len == 1 && seg[0] == '.') {
                continue;
            }
            if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
                path_pop(out_abs, &out_len, &depth, saved);
                continue;
            }

            int32_t rc = path_push(out_abs, VFS_PATH_MAX, &out_len, saved, &depth, (uint32_t)(sizeof(saved) / sizeof(saved[0])), seg, seg_len);
            if (rc < 0) {
                return rc;
            }
        }
    }

    // Apply `path` segments.
    p = path;
    while (*p == '/') p++;
    while (*p) {
        while (*p == '/') p++;
        if (*p == '\0') break;
        const char* seg = p;
        uint32_t seg_len = 0;
        while (seg[seg_len] && seg[seg_len] != '/') seg_len++;
        p += seg_len;

        if (seg_len == 1 && seg[0] == '.') {
            continue;
        }
        if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
            path_pop(out_abs, &out_len, &depth, saved);
            continue;
        }

        int32_t rc = path_push(out_abs, VFS_PATH_MAX, &out_len, saved, &depth, (uint32_t)(sizeof(saved) / sizeof(saved[0])), seg, seg_len);
        if (rc < 0) {
            return rc;
        }
    }

    if (out_len == 0) {
        out_abs[0] = '/';
        out_abs[1] = '\0';
    }
    return 0;
}

static void fat_ts_max_update(uint16_t* io_wtime, uint16_t* io_wdate, uint16_t wtime, uint16_t wdate) {
    if (!io_wtime || !io_wdate) {
        return;
    }
    uint32_t old_key = ((uint32_t)(*io_wdate) << 16) | (uint32_t)(*io_wtime);
    uint32_t new_key = ((uint32_t)wdate << 16) | (uint32_t)wtime;
    if (new_key > old_key) {
        *io_wtime = wtime;
        *io_wdate = wdate;
    }
}

static bool initramfs_lookup_mtime_abs(const char* abs_path, uint16_t* out_wtime, uint16_t* out_wdate) {
    if (out_wtime) *out_wtime = 0;
    if (out_wdate) *out_wdate = 0;
    if (!abs_path) {
        return false;
    }

    const char* rel = abs_path;
    while (*rel == '/') rel++;

    uint32_t n = vfs_file_count();
    for (uint32_t i = 0; i < n; i++) {
        const char* name = vfs_file_name(i);
        if (!name) continue;
        while (*name == '/') name++;
        if (ci_eq(name, rel)) {
            return vfs_file_mtime(i, out_wtime, out_wdate);
        }
    }
    return false;
}

static bool initramfs_max_mtime_under_abs(const char* abs_dir, uint16_t* out_wtime, uint16_t* out_wdate) {
    if (out_wtime) *out_wtime = 0;
    if (out_wdate) *out_wdate = 0;
    if (!abs_dir) {
        return false;
    }

    const char* rel = abs_dir;
    while (*rel == '/') rel++;
    uint32_t rel_len = (uint32_t)strlen(rel);

    bool found = false;
    uint32_t n = vfs_file_count();
    for (uint32_t i = 0; i < n; i++) {
        const char* name = vfs_file_name(i);
        if (!name) continue;
        while (*name == '/') name++;
        if (*name == '\0') continue;

        if (rel_len != 0) {
            if (!ci_starts_with(name, rel) || name[rel_len] != '/') {
                continue;
            }
        }

        uint16_t wtime = 0;
        uint16_t wdate = 0;
        (void)vfs_file_mtime(i, &wtime, &wdate);
        if (wdate == 0) {
            continue;
        }
        fat_ts_max_update(out_wtime, out_wdate, wtime, wdate);
        found = true;
    }

    return found;
}

static int32_t initramfs_stat_abs(const char* abs_path, vfs_stat_t* out) {
    if (!abs_path || !out) {
        return -EINVAL;
    }

    if (ci_eq(abs_path, "/")) {
        out->is_dir = 1;
        out->is_symlink = 0;
        out->mode = 0755;
        out->size = 0;
        out->wtime = 0;
        out->wdate = 0;
        return 0;
    }

    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (vfs_read_file(abs_path, &data, &size) && data) {
        out->is_dir = 0;
        out->is_symlink = 0;
        out->mode = 0644;
        out->size = size;
        (void)initramfs_lookup_mtime_abs(abs_path, &out->wtime, &out->wdate);
        return 0;
    }

    // Directory: any file under this prefix.
    const char* rel = abs_path;
    while (*rel == '/') rel++;
    if (*rel == '\0') {
        out->is_dir = 1;
        out->is_symlink = 0;
        out->mode = 0755;
        out->size = 0;
        out->wtime = 0;
        out->wdate = 0;
        return 0;
    }

    uint32_t rel_len = (uint32_t)strlen(rel);
    uint32_t n = vfs_file_count();
    for (uint32_t i = 0; i < n; i++) {
        const char* name = vfs_file_name(i);
        if (!name) continue;
        while (*name == '/') name++;
        if (ci_starts_with(name, rel) && name[rel_len] == '/') {
            out->is_dir = 1;
            out->is_symlink = 0;
            out->mode = 0755;
            out->size = 0;
            (void)initramfs_max_mtime_under_abs(abs_path, &out->wtime, &out->wdate);
            return 0;
        }
    }

    // Mountpoints exposed at root even if initramfs doesn't have them.
    if (ci_eq(abs_path, "/ram")) {
        out->is_dir = 1;
        out->is_symlink = 0;
        out->mode = 0755;
        out->size = 0;
        out->wtime = 0;
        out->wdate = 0;
        return 0;
    }
    if (ci_eq(abs_path, "/disk")) {
        out->is_dir = 1;
        out->is_symlink = 0;
        out->mode = 0755;
        out->size = 0;
        out->wtime = 0;
        out->wdate = 0;
        return 0;
    }

    return -ENOENT;
}

static uint32_t add_unique_dirent(vfs_dirent_t* out, uint32_t count, uint32_t max, const char* name, bool is_dir, uint32_t size,
                                  uint16_t wtime, uint16_t wdate) {
    if (!out || !name || name[0] == '\0') {
        return count;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (ci_eq(out[i].name, name)) {
            if (is_dir) {
                out[i].is_dir = 1;
                out[i].size = 0;
                out[i].mode = 0755;
            }
            if (wdate != 0) {
                fat_ts_max_update(&out[i].wtime, &out[i].wdate, wtime, wdate);
            }
            return count;
        }
    }
    if (count >= max) {
        return count;
    }
    strncpy(out[count].name, name, VFS_NAME_MAX - 1u);
    out[count].name[VFS_NAME_MAX - 1u] = '\0';
    out[count].is_dir = is_dir ? 1u : 0u;
    out[count].is_symlink = 0;
    out[count].mode = is_dir ? 0755 : 0644;
    out[count].size = is_dir ? 0 : size;
    out[count].wtime = wtime;
    out[count].wdate = wdate;
    return count + 1u;
}

static uint32_t initramfs_list_dir_abs(const char* abs_path, vfs_dirent_t* out, uint32_t max) {
    if (!abs_path || !out || max == 0) {
        return 0;
    }

    // Compute rel prefix without leading '/'.
    const char* rel = abs_path;
    while (*rel == '/') rel++;
    char dir_rel[VFS_PATH_MAX];
    dir_rel[0] = '\0';

    if (ci_eq(abs_path, "/")) {
        dir_rel[0] = '\0'; // root
    } else {
        strncpy(dir_rel, rel, sizeof(dir_rel) - 1u);
        dir_rel[sizeof(dir_rel) - 1u] = '\0';
    }

    // Drop trailing '/'.
    uint32_t dir_len = (uint32_t)strlen(dir_rel);
    while (dir_len > 0 && dir_rel[dir_len - 1u] == '/') {
        dir_rel[--dir_len] = '\0';
    }

    uint32_t count = 0;

    // Root mountpoints.
    if (dir_len == 0) {
        uint16_t wtime = 0;
        uint16_t wdate = 0;
        bool is_dir = false;
        uint32_t size = 0;

        // Add /disk mount point (Minix filesystem)
        if (minixfs_is_ready()) {
            minixfs_stat_t mst;
            if (minixfs_stat("/", &mst)) {
                unix_to_fat_ts(mst.mtime, &wtime, &wdate);
            }
            count = add_unique_dirent(out, count, max, "disk", true, 0, wtime, wdate);

            // Check if /disk/usr exists for /usr alias
            if (minixfs_stat("/usr", &mst)) {
                unix_to_fat_ts(mst.mtime, &wtime, &wdate);
                count = add_unique_dirent(out, count, max, "usr", true, 0, wtime, wdate);
            }
        }

        wtime = 0;
        wdate = 0;
        is_dir = false;
        size = 0;
        (void)ramfs_stat_ex("/ram", &is_dir, &size, &wtime, &wdate);
        count = add_unique_dirent(out, count, max, "ram", true, 0, wtime, wdate);
    }

    uint32_t n = vfs_file_count();
    for (uint32_t i = 0; i < n && count < max; i++) {
        const char* name = vfs_file_name(i);
        if (!name) continue;
        while (*name == '/') name++;
        if (*name == '\0') continue;

        const char* child = name;
        if (dir_len != 0) {
            if (!ci_starts_with(name, dir_rel) || name[dir_len] != '/') {
                continue;
            }
            child = name + dir_len + 1u;
        }
        if (!child || child[0] == '\0') {
            continue;
        }

        // Extract first path segment.
        char seg[VFS_NAME_MAX];
        uint32_t seg_len = 0;
        while (child[seg_len] && child[seg_len] != '/' && seg_len + 1u < sizeof(seg)) {
            seg[seg_len] = child[seg_len];
            seg_len++;
        }
        seg[seg_len] = '\0';
        if (seg_len == 0) {
            continue;
        }

        bool is_dir = child[seg_len] == '/';
        uint32_t size = is_dir ? 0u : vfs_file_size(i);
        uint16_t wtime = 0;
        uint16_t wdate = 0;
        (void)vfs_file_mtime(i, &wtime, &wdate);
        count = add_unique_dirent(out, count, max, seg, is_dir, size, wtime, wdate);
    }

    return count;
}

enum {
    VFS_SYMLINK_MAX_DEPTH = 8,
};

static int32_t vfs_lstat_abs(const char* abs_path, vfs_stat_t* out) {
    if (!abs_path || abs_path[0] != '/' || !out) {
        return -EINVAL;
    }
    memset(out, 0, sizeof(*out));

    // /disk uses Minix filesystem for persistent storage
    if (abs_is_mount(abs_path, "/disk")) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* rel = abs_path + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";
        minixfs_stat_t mst;
        if (!minixfs_stat(rel, &mst)) {
            return -ENOENT;
        }
        out->is_dir = MINIX_S_ISDIR(mst.mode) ? 1u : 0u;
        out->is_symlink = MINIX_S_ISLNK(mst.mode) ? 1u : 0u;
        out->size = mst.size;
        out->mode = (uint16_t)(mst.mode & 07777u);
        out->uid = mst.uid;
        out->gid = mst.gid;
        unix_to_fat_ts(mst.mtime, &out->wtime, &out->wdate);
        return 0;
    }

    if (abs_is_mount(abs_path, "/ram")) {
        bool is_dir = false;
        uint32_t size = 0;
        uint16_t wtime = 0;
        uint16_t wdate = 0;
        if (!ramfs_stat_ex(abs_path, &is_dir, &size, &wtime, &wdate)) {
            return -ENOENT;
        }
        out->is_dir = is_dir ? 1u : 0u;
        out->size = size;
        out->wtime = wtime;
        out->wdate = wdate;

        bool is_symlink = false;
        uint16_t mode = is_dir ? 0755u : 0644u;
        (void)ramfs_get_meta(abs_path, &is_symlink, &mode);
        out->is_symlink = is_symlink ? 1u : 0u;
        out->mode = (uint16_t)(mode & 07777u);
        if (out->is_symlink) {
            out->is_dir = 0;
        }

        // Get owner uid/gid.
        uint32_t uid = 0, gid = 0;
        (void)ramfs_get_owner(abs_path, &uid, &gid);
        out->uid = uid;
        out->gid = gid;
        return 0;
    }

    return initramfs_stat_abs(abs_path, out);
}

static int32_t vfs_readlink_abs(const char* abs_path, char* out, uint32_t cap, uint32_t* out_len) {
    if (out_len) {
        *out_len = 0;
    }
    if (!abs_path || abs_path[0] != '/' || (cap != 0 && !out)) {
        return -EINVAL;
    }

    vfs_stat_t st;
    int32_t rc = vfs_lstat_abs(abs_path, &st);
    if (rc < 0) {
        return rc;
    }
    if (!st.is_symlink) {
        return -EINVAL;
    }

    if (cap == 0) {
        return 0;
    }

    // /disk uses Minix filesystem
    if (abs_is_mount(abs_path, "/disk")) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* rel = abs_path + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";
        char target[VFS_PATH_MAX];
        if (!minixfs_readlink(rel, target, sizeof(target))) {
            return -EIO;
        }
        uint32_t size = (uint32_t)strlen(target);
        uint32_t n = size;
        if (n > cap) {
            n = cap;
        }
        if (n != 0) {
            memcpy(out, target, n);
        }
        if (out_len) {
            *out_len = size;
        }
        return (int32_t)n;
    }

    if (abs_is_mount(abs_path, "/ram")) {
        const uint8_t* data = NULL;
        uint32_t size = 0;
        if (!ramfs_read_file(abs_path, &data, &size)) {
            return -ENOENT;
        }
        uint32_t n = size;
        if (n > cap) {
            n = cap;
        }
        if (n != 0 && data) {
            memcpy(out, data, n);
        }
        if (out_len) {
            *out_len = size;
        }
        return (int32_t)n;
    }

    return -EINVAL;
}

static int32_t vfs_expand_symlink_target(const char* link_abs, const char* target, const char* rest, char out[VFS_PATH_MAX]) {
    if (!out) {
        return -EINVAL;
    }
    out[0] = '\0';
    if (!link_abs || link_abs[0] != '/' || !target || !rest) {
        return -EINVAL;
    }

    char combined[VFS_PATH_MAX];
    combined[0] = '\0';

    if (target[0] == '/') {
        size_t tlen = strlen(target);
        size_t rlen = strlen(rest);
        if (tlen + rlen + 1u > sizeof(combined)) {
            return -ENAMETOOLONG;
        }
        memcpy(combined, target, tlen);
        memcpy(combined + tlen, rest, rlen);
        combined[tlen + rlen] = '\0';
    } else {
        char dir[VFS_PATH_MAX];
        abs_dirname(link_abs, dir);
        size_t dlen = strlen(dir);
        size_t tlen = strlen(target);
        size_t rlen = strlen(rest);

        // dir + '/' + target + rest
        size_t need = dlen + ((dlen > 1u) ? 1u : 0u) + tlen + rlen + 1u;
        if (need > sizeof(combined)) {
            return -ENAMETOOLONG;
        }
        size_t pos = 0;
        memcpy(combined + pos, dir, dlen);
        pos += dlen;
        if (dlen > 1u) {
            combined[pos++] = '/';
        }
        memcpy(combined + pos, target, tlen);
        pos += tlen;
        memcpy(combined + pos, rest, rlen);
        pos += rlen;
        combined[pos] = '\0';
    }

    char canon[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve("/", combined, canon);
    if (rc < 0) {
        return rc;
    }
    char canon_usr[VFS_PATH_MAX];
    const char* eff = abs_apply_posix_aliases(canon, canon_usr);
    strncpy(out, eff, VFS_PATH_MAX - 1u);
    out[VFS_PATH_MAX - 1u] = '\0';
    return 0;
}

static int32_t vfs_resolve_symlinks_abs(const char* abs_in, bool follow_final, char out[VFS_PATH_MAX]) {
    if (!out) {
        return -EINVAL;
    }
    out[0] = '\0';
    if (!abs_in || abs_in[0] != '/') {
        return -EINVAL;
    }

    char cur[VFS_PATH_MAX];
    strncpy(cur, abs_in, sizeof(cur) - 1u);
    cur[sizeof(cur) - 1u] = '\0';

    for (int depth = 0; depth < VFS_SYMLINK_MAX_DEPTH; depth++) {
        bool changed = false;

        // Scan segments left-to-right and expand the first symlink we see.
        uint32_t i = 1; // skip leading '/'
        while (cur[i] != '\0') {
            // Find end of this segment.
            uint32_t seg_start = i;
            while (cur[i] != '\0' && cur[i] != '/') {
                i++;
            }
            uint32_t seg_end = i;

            bool last = cur[i] == '\0';
            if (!last) {
                // Skip '/' for the next iteration.
                i++;
            }

            char prefix[VFS_PATH_MAX];
            if (seg_end >= sizeof(prefix)) {
                return -ENAMETOOLONG;
            }
            memcpy(prefix, cur, seg_end);
            prefix[seg_end] = '\0';

            // Avoid lstat'ing a partial mountpoint token.
            if (ci_eq(prefix, "")) {
                continue;
            }

            vfs_stat_t st;
            int32_t rc = vfs_lstat_abs(prefix, &st);
            if (rc < 0) {
                return rc;
            }

            if (st.is_symlink && (follow_final || !last)) {
                char target[VFS_PATH_MAX];
                uint32_t target_len = 0;
                rc = vfs_readlink_abs(prefix, target, sizeof(target) - 1u, &target_len);
                if (rc < 0) {
                    return rc;
                }
                // We need the full link target to resolve paths correctly.
                if (target_len >= sizeof(target) || (uint32_t)rc != target_len) {
                    return -ENAMETOOLONG;
                }
                target[target_len] = '\0';

                const char* rest = cur + seg_end;
                char next[VFS_PATH_MAX];
                rc = vfs_expand_symlink_target(prefix, target, rest, next);
                if (rc < 0) {
                    return rc;
                }

                strncpy(cur, next, sizeof(cur) - 1u);
                cur[sizeof(cur) - 1u] = '\0';
                changed = true;
                break;
            }
        }

        if (!changed) {
            strncpy(out, cur, VFS_PATH_MAX - 1u);
            out[VFS_PATH_MAX - 1u] = '\0';
            return 0;
        }
    }

    return -ELOOP;
}

static int32_t vfs_prepare_existing_path(const char* cwd, const char* path, bool follow_final, char out[VFS_PATH_MAX]) {
    if (!out) {
        return -EINVAL;
    }
    out[0] = '\0';

    char abs[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve(cwd, path, abs);
    if (rc < 0) {
        return rc;
    }

    char abs_usr[VFS_PATH_MAX];
    const char* eff = abs_apply_posix_aliases(abs, abs_usr);
    return vfs_resolve_symlinks_abs(eff, follow_final, out);
}

static int32_t vfs_prepare_create_path(const char* cwd, const char* path, char out[VFS_PATH_MAX]) {
    if (!out) {
        return -EINVAL;
    }
    out[0] = '\0';

    char abs[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve(cwd, path, abs);
    if (rc < 0) {
        return rc;
    }

    char abs_usr[VFS_PATH_MAX];
    const char* eff = abs_apply_posix_aliases(abs, abs_usr);
    const char* base = strrchr(eff, '/');
    base = base ? base + 1 : eff;
    if (!base || base[0] == '\0') {
        return -EINVAL;
    }

    char parent[VFS_PATH_MAX];
    abs_dirname(eff, parent);

    char parent_res[VFS_PATH_MAX];
    rc = vfs_resolve_symlinks_abs(parent, true, parent_res);
    if (rc < 0) {
        return rc;
    }

    vfs_stat_t st;
    rc = vfs_lstat_abs(parent_res, &st);
    if (rc < 0) {
        return rc;
    }
    if (!st.is_dir) {
        return -ENOTDIR;
    }

    size_t plen = strlen(parent_res);
    size_t blen = strlen(base);
    size_t need = plen + ((plen > 1u) ? 1u : 0u) + blen + 1u;
    if (need > VFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }
    size_t pos = 0;
    memcpy(out + pos, parent_res, plen);
    pos += plen;
    if (plen > 1u) {
        out[pos++] = '/';
    }
    memcpy(out + pos, base, blen);
    pos += blen;
    out[pos] = '\0';
    return 0;
}

int32_t vfs_lstat_path(const char* cwd, const char* path, vfs_stat_t* out) {
    if (!out) {
        return -EINVAL;
    }
    char eff[VFS_PATH_MAX];
    int32_t rc = vfs_prepare_existing_path(cwd, path, false, eff);
    if (rc < 0) {
        return rc;
    }
    return vfs_lstat_abs(eff, out);
}

int32_t vfs_stat_path(const char* cwd, const char* path, vfs_stat_t* out) {
    if (!out) {
        return -EINVAL;
    }
    char eff[VFS_PATH_MAX];
    int32_t rc = vfs_prepare_existing_path(cwd, path, true, eff);
    if (rc < 0) {
        return rc;
    }
    return vfs_lstat_abs(eff, out);
}

int32_t vfs_mkdir_path(const char* cwd, const char* path) {
    char abs[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve(cwd, path, abs);
    if (rc < 0) {
        return rc;
    }

    if (ci_eq(abs, "/")) {
        return -EEXIST;
    }

    char eff[VFS_PATH_MAX];
    rc = vfs_prepare_create_path(cwd, path, eff);
    if (rc < 0) {
        return rc;
    }

    // /disk uses Minix filesystem
    if (abs_is_mount(eff, "/disk")) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* rel = eff + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";
        if (minixfs_is_dir(rel) || minixfs_is_file(rel)) {
            return -EEXIST;
        }
        if (!minixfs_mkdir(rel)) {
            return -EIO;
        }
        return 0;
    }

    if (abs_is_mount(eff, "/ram")) {
        if (ramfs_is_dir(eff) || ramfs_is_file(eff)) {
            return -EEXIST;
        }
        if (!ramfs_mkdir(eff)) {
            return -EIO;
        }
        return 0;
    }

    // initramfs is read-only.
    return -EROFS;
}

int32_t vfs_symlink_path(const char* cwd, const char* target, const char* linkpath) {
    if (!target || !linkpath) {
        return -EINVAL;
    }

    char abs[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve(cwd, linkpath, abs);
    if (rc < 0) {
        return rc;
    }
    if (ci_eq(abs, "/") || ci_eq(abs, "/ram") || ci_eq(abs, "/disk") || ci_eq(abs, "/usr")) {
        return -EPERM;
    }

    char eff[VFS_PATH_MAX];
    rc = vfs_prepare_create_path(cwd, linkpath, eff);
    if (rc < 0) {
        return rc;
    }

    uint32_t len = (uint32_t)strlen(target);

    // /disk uses Minix filesystem
    if (abs_is_mount(eff, "/disk")) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* rel = eff + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";
        if (minixfs_is_dir(rel) || minixfs_is_file(rel)) {
            return -EEXIST;
        }
        if (!minixfs_symlink(target, rel)) {
            return -EIO;
        }
        return 0;
    }

    if (abs_is_mount(eff, "/ram")) {
        if (ramfs_is_dir(eff) || ramfs_is_file(eff)) {
            return -EEXIST;
        }
        if (!ramfs_write_file(eff, (const uint8_t*)target, len, false)) {
            return -EIO;
        }
        if (!ramfs_set_meta(eff, true, 0777u)) {
            (void)ramfs_unlink(eff);
            return -EIO;
        }
        return 0;
    }

    return -EROFS;
}

int32_t vfs_readlink_path(const char* cwd, const char* path, char* out, uint32_t cap) {
    char eff[VFS_PATH_MAX];
    int32_t rc = vfs_prepare_existing_path(cwd, path, false, eff);
    if (rc < 0) {
        return rc;
    }
    return vfs_readlink_abs(eff, out, cap, NULL);
}

int32_t vfs_chmod_path(const char* cwd, const char* path, uint16_t mode) {
    char eff[VFS_PATH_MAX];
    int32_t rc = vfs_prepare_existing_path(cwd, path, true, eff);
    if (rc < 0) {
        return rc;
    }

    mode &= 07777u;

    vfs_stat_t st;
    rc = vfs_lstat_abs(eff, &st);
    if (rc < 0) {
        return rc;
    }
    if (st.is_symlink) {
        return -EINVAL;
    }

    // /disk uses Minix filesystem
    if (abs_is_mount(eff, "/disk")) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* rel = eff + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";
        if (!minixfs_chmod(rel, mode)) {
            return -EIO;
        }
        return 0;
    }

    if (abs_is_mount(eff, "/ram")) {
        if (!ramfs_set_meta(eff, false, mode)) {
            return -EIO;
        }
        return 0;
    }

    return -EROFS;
}

int32_t vfs_chown_path(const char* cwd, const char* path, uint32_t uid, uint32_t gid) {
    char eff[VFS_PATH_MAX];
    int32_t rc = vfs_prepare_existing_path(cwd, path, true, eff);
    if (rc < 0) {
        return rc;
    }

    // /disk uses Minix filesystem
    if (abs_is_mount(eff, "/disk")) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* rel = eff + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";
        if (!minixfs_chown(rel, (uint16_t)uid, (uint16_t)gid)) {
            return -EIO;
        }
        return 0;
    }

    if (abs_is_mount(eff, "/ram")) {
        if (!ramfs_set_owner(eff, uid, gid)) {
            return -EIO;
        }
        return 0;
    }

    return -EROFS;
}

int32_t vfs_lchown_path(const char* cwd, const char* path, uint32_t uid, uint32_t gid) {
    char eff[VFS_PATH_MAX];
    // Don't follow symlinks for lchown.
    int32_t rc = vfs_prepare_existing_path(cwd, path, false, eff);
    if (rc < 0) {
        return rc;
    }

    // /disk uses Minix filesystem
    if (abs_is_mount(eff, "/disk")) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* rel = eff + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";
        if (!minixfs_chown(rel, (uint16_t)uid, (uint16_t)gid)) {
            return -EIO;
        }
        return 0;
    }

    if (abs_is_mount(eff, "/ram")) {
        if (!ramfs_set_owner(eff, uid, gid)) {
            return -EIO;
        }
        return 0;
    }

    return -EROFS;
}

int32_t vfs_statfs_path(const char* cwd, const char* path, vfs_statfs_t* out) {
    if (!out) {
        return -EINVAL;
    }
    memset(out, 0, sizeof(*out));

    char eff[VFS_PATH_MAX];
    int32_t rc = vfs_prepare_existing_path(cwd, path, true, eff);
    if (rc < 0) {
        return rc;
    }

    // /disk uses Minix filesystem
    if (abs_is_mount(eff, "/disk")) {
        uint32_t total_blocks = 0;
        uint32_t free_blocks = 0;
        uint32_t total_inodes = 0;
        uint32_t free_inodes = 0;
        if (!minixfs_statfs(&total_blocks, &free_blocks, &total_inodes, &free_inodes)) {
            return -EIO;
        }
        out->bsize = MINIX_BLOCK_SIZE;
        out->blocks = total_blocks;
        out->bfree = free_blocks;
        out->bavail = free_blocks;
        return 0;
    }

    if (abs_is_mount(eff, "/ram")) {
        uint32_t bsize = PAGE_SIZE;
        uint32_t blocks = pmm_total_frames();
        uint32_t bfree = pmm_free_frames();
        out->bsize = bsize;
        out->blocks = blocks;
        out->bfree = bfree;
        out->bavail = bfree;
        return 0;
    }

    // initramfs: read-only; report total packed bytes and no free space.
    uint32_t total = 0;
    uint32_t n = vfs_file_count();
    for (uint32_t i = 0; i < n; i++) {
        total += vfs_file_size(i);
    }

    out->bsize = 512u;
    out->blocks = (total + out->bsize - 1u) / out->bsize;
    out->bfree = 0;
    out->bavail = 0;
    return 0;
}

static int32_t handle_alloc(vfs_handle_t** out) {
    if (!out) {
        return -EINVAL;
    }
    vfs_handle_t* h = (vfs_handle_t*)kcalloc(1, sizeof(vfs_handle_t));
    if (!h) {
        return -ENOMEM;
    }
    h->refcount = 1;
    *out = h;
    return 0;
}

static int32_t open_dir_handle(vfs_backend_t backend, const char* abs_path, uint32_t flags, vfs_handle_t** out) {
    (void)flags;
    vfs_handle_t* h = NULL;
    int32_t rc = handle_alloc(&h);
    if (rc < 0) {
        return rc;
    }

    h->kind = VFS_HANDLE_DIR;
    h->backend = backend;
    h->flags = flags;
    h->off = 0;
    strncpy(h->abs_path, abs_path ? abs_path : "/", sizeof(h->abs_path) - 1u);
    h->abs_path[sizeof(h->abs_path) - 1u] = '\0';

    // Materialize directory entries.
    uint32_t count = 0;

    if (backend == VFS_BACKEND_MINIXFS) {
        minixfs_dirent_t* dents = (minixfs_dirent_t*)kcalloc(VFS_MAX_DIR_ENTRIES, sizeof(minixfs_dirent_t));
        if (!dents) {
            kfree(h);
            return -ENOMEM;
        }
        // abs_path is like "/disk/foo", skip "/disk"
        const char* rel = abs_path;
        if (ci_starts_with(abs_path, "/disk")) {
            rel = abs_path + 5;
            if (*rel == '\0') rel = "/";
        }
        count = minixfs_readdir(rel, dents, VFS_MAX_DIR_ENTRIES);
        if (count > 0) {
            h->ents = (vfs_dirent_t*)kcalloc(count, sizeof(vfs_dirent_t));
            if (!h->ents) {
                kfree(dents);
                kfree(h);
                return -ENOMEM;
            }
            for (uint32_t i = 0; i < count; i++) {
                strncpy(h->ents[i].name, dents[i].name, VFS_NAME_MAX - 1u);
                h->ents[i].name[VFS_NAME_MAX - 1u] = '\0';
                h->ents[i].is_dir = dents[i].is_dir ? 1u : 0u;
                // Get full info via stat
                char full_path[VFS_PATH_MAX];
                if (rel[0] == '/' && rel[1] == '\0') {
                    full_path[0] = '/';
                    strncpy(full_path + 1, dents[i].name, VFS_PATH_MAX - 2);
                    full_path[VFS_PATH_MAX - 1] = '\0';
                } else {
                    strncpy(full_path, rel, VFS_PATH_MAX - 1);
                    full_path[VFS_PATH_MAX - 1] = '\0';
                    size_t len = strlen(full_path);
                    if (len + 2 < VFS_PATH_MAX) {
                        full_path[len] = '/';
                        strncpy(full_path + len + 1, dents[i].name, VFS_PATH_MAX - len - 2);
                        full_path[VFS_PATH_MAX - 1] = '\0';
                    }
                }
                minixfs_stat_t mst;
                if (minixfs_stat(full_path, &mst)) {
                    h->ents[i].is_symlink = MINIX_S_ISLNK(mst.mode) ? 1u : 0u;
                    h->ents[i].mode = (uint16_t)(mst.mode & 07777u);
                    h->ents[i].size = mst.size;
                }
            }
        }
        kfree(dents);
    } else if (backend == VFS_BACKEND_RAMFS) {
        ramfs_dirent_t* dents = (ramfs_dirent_t*)kcalloc(VFS_MAX_DIR_ENTRIES, sizeof(ramfs_dirent_t));
        if (!dents) {
            kfree(h);
            return -ENOMEM;
        }
        count = ramfs_list_dir(abs_path, dents, VFS_MAX_DIR_ENTRIES);
        if (count > 0) {
            h->ents = (vfs_dirent_t*)kcalloc(count, sizeof(vfs_dirent_t));
            if (!h->ents) {
                kfree(dents);
                kfree(h);
                return -ENOMEM;
            }
            for (uint32_t i = 0; i < count; i++) {
                strncpy(h->ents[i].name, dents[i].name, VFS_NAME_MAX - 1u);
                h->ents[i].name[VFS_NAME_MAX - 1u] = '\0';
                h->ents[i].is_dir = dents[i].is_dir ? 1u : 0u;
                h->ents[i].is_symlink = dents[i].is_symlink ? 1u : 0u;
                h->ents[i].mode = (uint16_t)(dents[i].mode & 07777u);
                h->ents[i].size = dents[i].size;
                h->ents[i].wtime = dents[i].wtime;
                h->ents[i].wdate = dents[i].wdate;
            }
        }
        kfree(dents);
    } else {
        vfs_dirent_t* tmp = (vfs_dirent_t*)kcalloc(VFS_MAX_DIR_ENTRIES, sizeof(vfs_dirent_t));
        if (!tmp) {
            kfree(h);
            return -ENOMEM;
        }
        count = initramfs_list_dir_abs(abs_path, tmp, VFS_MAX_DIR_ENTRIES);
        if (count > 0) {
            h->ents = (vfs_dirent_t*)kcalloc(count, sizeof(vfs_dirent_t));
            if (!h->ents) {
                kfree(tmp);
                kfree(h);
                return -ENOMEM;
            }
            memcpy(h->ents, tmp, (size_t)count * sizeof(vfs_dirent_t));
        }
        kfree(tmp);
    }
    h->ent_count = count;
    h->ent_index = 0;

    *out = h;
    return 0;
}

static int32_t open_file_handle(vfs_backend_t backend, const char* abs_path, uint32_t flags, const uint8_t* data, uint32_t size, vfs_handle_t** out) {
    vfs_handle_t* h = NULL;
    int32_t rc = handle_alloc(&h);
    if (rc < 0) {
        return rc;
    }

    h->kind = VFS_HANDLE_FILE;
    h->backend = backend;
    h->flags = flags;
    h->off = 0;
    strncpy(h->abs_path, abs_path ? abs_path : "/", sizeof(h->abs_path) - 1u);
    h->abs_path[sizeof(h->abs_path) - 1u] = '\0';

    h->ro_data = data;
    h->buf = NULL;
    h->size = size;
    h->cap = 0;
    h->dirty = false;

    if ((flags & VFS_O_APPEND) != 0) {
        h->off = h->size;
    }

    *out = h;
    return 0;
}

static bool handle_writable(const vfs_handle_t* h) {
    if (!h) return false;
    uint32_t acc = h->flags & VFS_O_ACCMODE;
    return acc == VFS_O_WRONLY || acc == VFS_O_RDWR;
}

static int32_t handle_ensure_buf(vfs_handle_t* h) {
    if (!h) {
        return -EINVAL;
    }
    if (h->buf) {
        return 0;
    }

    uint32_t cap = h->size ? h->size : 1u;
    uint8_t* buf = (uint8_t*)kmalloc(cap);
    if (!buf) {
        return -ENOMEM;
    }
    if (h->size && h->ro_data) {
        memcpy(buf, h->ro_data, h->size);
    } else if (h->size) {
        // Shouldn't happen (size without source), but keep it defined.
        memset(buf, 0, h->size);
    } else {
        buf[0] = 0;
    }
    h->buf = buf;
    h->cap = cap;
    h->ro_data = h->buf; // point reads at the mutable buffer
    return 0;
}

static int32_t handle_grow(vfs_handle_t* h, uint32_t needed) {
    if (!h) {
        return -EINVAL;
    }
    if (needed <= h->cap) {
        return 0;
    }

    uint32_t new_cap = h->cap ? h->cap : 1u;
    while (new_cap < needed) {
        uint32_t next = new_cap * 2u;
        if (next < new_cap) {
            new_cap = needed;
            break;
        }
        new_cap = next;
    }

    uint8_t* nb = (uint8_t*)kmalloc(new_cap);
    if (!nb) {
        return -ENOMEM;
    }
    if (h->size && h->buf) {
        memcpy(nb, h->buf, h->size);
    } else if (h->size && h->ro_data) {
        memcpy(nb, h->ro_data, h->size);
    }
    if (h->buf) {
        kfree(h->buf);
    }
    h->buf = nb;
    h->cap = new_cap;
    h->ro_data = h->buf;
    return 0;
}

int32_t vfs_open_path(const char* cwd, const char* path, uint32_t flags, vfs_handle_t** out) {
    if (!out) {
        return -EINVAL;
    }

    uint32_t acc = flags & VFS_O_ACCMODE;
    bool want_write = (acc == VFS_O_WRONLY || acc == VFS_O_RDWR);
    bool want_dir = (flags & VFS_O_DIRECTORY) != 0;

    char abs[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve(cwd, path, abs);
    if (rc < 0) {
        return rc;
    }

    char abs_usr[VFS_PATH_MAX];
    const char* aliased = abs_apply_posix_aliases(abs, abs_usr);

    // Resolve intermediate symlinks, but do not follow the final segment yet.
    // This lets us:
    // - decide whether the pathname itself exists (O_EXCL semantics),
    // - detect and follow a final symlink explicitly,
    // - create a missing target of a final symlink when O_CREAT is set.
    char eff[VFS_PATH_MAX];
    rc = vfs_resolve_symlinks_abs(aliased, false, eff);
    if (rc == 0) {
        vfs_stat_t st;
        rc = vfs_lstat_abs(eff, &st);
        if (rc < 0) {
            return rc;
        }

        if ((flags & VFS_O_CREAT) && (flags & VFS_O_EXCL)) {
            return -EEXIST;
        }

        if (st.is_symlink) {
            char target[VFS_PATH_MAX];
            uint32_t target_len = 0;
            int32_t n = vfs_readlink_abs(eff, target, sizeof(target) - 1u, &target_len);
            if (n < 0) {
                return n;
            }
            // Need the full target path for correct resolution.
            if (target_len >= sizeof(target) || (uint32_t)n != target_len) {
                return -ENAMETOOLONG;
            }
            target[target_len] = '\0';

            char target_abs[VFS_PATH_MAX];
            rc = vfs_expand_symlink_target(eff, target, "", target_abs);
            if (rc < 0) {
                return rc;
            }

            // Follow the link and open (creating the target if requested).
            return vfs_open_path("/", target_abs, flags, out);
        }
    } else if (rc == -ENOENT) {
        if ((flags & VFS_O_CREAT) == 0) {
            return -ENOENT;
        }
        if (want_dir) {
            return -ENOENT;
        }

        const char* base = strrchr(aliased, '/');
        base = base ? base + 1 : aliased;
        if (!base || base[0] == '\0') {
            return -EINVAL;
        }

        char parent[VFS_PATH_MAX];
        abs_dirname(aliased, parent);

        char parent_res[VFS_PATH_MAX];
        rc = vfs_resolve_symlinks_abs(parent, true, parent_res);
        if (rc < 0) {
            return rc;
        }

        vfs_stat_t pst;
        rc = vfs_lstat_abs(parent_res, &pst);
        if (rc < 0) {
            return rc;
        }
        if (!pst.is_dir) {
            return -ENOTDIR;
        }

        size_t plen = strlen(parent_res);
        size_t blen = strlen(base);
        size_t need = plen + ((plen > 1u) ? 1u : 0u) + blen + 1u;
        if (need > VFS_PATH_MAX) {
            return -ENAMETOOLONG;
        }

        size_t pos = 0;
        memcpy(eff + pos, parent_res, plen);
        pos += plen;
        if (plen > 1u) {
            eff[pos++] = '/';
        }
        memcpy(eff + pos, base, blen);
        pos += blen;
        eff[pos] = '\0';
    } else if (rc < 0) {
        return rc;
    }

    // /disk (minixfs)
    if (abs_is_mount(eff, "/disk")) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* rel = eff + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";

        bool is_dir = minixfs_is_dir(rel);
        bool is_file = minixfs_is_file(rel);

        if (is_dir || is_file) {
            if (want_dir && !is_dir) {
                return -ENOTDIR;
            }
            if (is_dir) {
                if (want_write) {
                    return -EISDIR;
                }
                return open_dir_handle(VFS_BACKEND_MINIXFS, eff, flags, out);
            }
            if ((flags & VFS_O_CREAT) && (flags & VFS_O_EXCL)) {
                return -EEXIST;
            }
            if (want_write && (flags & VFS_O_TRUNC)) {
                if (!minixfs_write_file(rel, NULL, 0)) {
                    return -EIO;
                }
            }
        } else {
            if (want_dir) {
                return -ENOENT;
            }
            if ((flags & VFS_O_CREAT) == 0) {
                return -ENOENT;
            }
            // Create empty file.
            if (!minixfs_write_file(rel, NULL, 0)) {
                return -EIO;
            }
        }

        uint8_t* data = NULL;
        uint32_t size = 0;
        if ((flags & VFS_O_TRUNC) == 0) {
            data = minixfs_read_file(rel, &size);
            // data may be NULL for empty files
        }

        rc = open_file_handle(VFS_BACKEND_MINIXFS, eff, flags, data, size, out);
        if (rc < 0) {
            if (data) kfree(data);
            return rc;
        }
        (*out)->buf = data;
        (*out)->cap = size;
        (*out)->ro_data = (*out)->buf;
        return 0;
    }

    // /ram
    if (abs_is_mount(eff, "/ram")) {
        bool is_dir = ramfs_is_dir(eff);
        bool is_file = ramfs_is_file(eff);

        if (is_dir || is_file) {
            if (want_dir && !is_dir) {
                return -ENOTDIR;
            }
            if (is_dir) {
                if (want_write) {
                    return -EISDIR;
                }
                return open_dir_handle(VFS_BACKEND_RAMFS, eff, flags, out);
            }
            if ((flags & VFS_O_CREAT) && (flags & VFS_O_EXCL)) {
                return -EEXIST;
            }
            if (want_write && (flags & VFS_O_TRUNC)) {
                if (!ramfs_write_file(eff, NULL, 0, true)) {
                    return -EIO;
                }
            }
        } else {
            if (want_dir) {
                return -ENOENT;
            }
            if ((flags & VFS_O_CREAT) == 0) {
                return -ENOENT;
            }
            if (!ramfs_write_file(eff, NULL, 0, false)) {
                return -EIO;
            }
        }

        const uint8_t* ro = NULL;
        uint32_t size = 0;
        if ((flags & VFS_O_TRUNC) == 0) {
            (void)ramfs_read_file(eff, &ro, &size);
        }

        return open_file_handle(VFS_BACKEND_RAMFS, eff, flags, ro, size, out);
    }

    // initramfs (read-only)
    if (want_write || (flags & VFS_O_CREAT)) {
        return -EROFS;
    }

    vfs_stat_t st;
    rc = initramfs_stat_abs(eff, &st);
    if (rc < 0) {
        return rc;
    }
    if (want_dir && !st.is_dir) {
        return -ENOTDIR;
    }
    if (st.is_dir) {
        return open_dir_handle(VFS_BACKEND_INITRAMFS, eff, flags, out);
    }

    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (!vfs_read_file(eff, &data, &size) || !data) {
        return -ENOENT;
    }
    return open_file_handle(VFS_BACKEND_INITRAMFS, eff, flags, data, size, out);
}

void vfs_ref(vfs_handle_t* h) {
    if (!h) {
        return;
    }
    if (h->refcount == 0) {
        h->refcount = 1;
        return;
    }
    h->refcount++;
}

int32_t vfs_close(vfs_handle_t* h) {
    if (!h) {
        return -EINVAL;
    }

    if (h->refcount > 1u) {
        h->refcount--;
        return 0;
    }

    int32_t rc = 0;
    if (h->kind == VFS_HANDLE_FILE && h->dirty && handle_writable(h)) {
        if (h->backend == VFS_BACKEND_MINIXFS) {
            // h->abs_path is "/disk/...", extract relative path
            const char* rel = h->abs_path + 5;
            if (*rel == '\0') rel = "/";
            if (!minixfs_write_file(rel, h->buf, h->size)) {
                rc = -EIO;
            }
        } else if (h->backend == VFS_BACKEND_RAMFS) {
            if (!ramfs_write_file(h->abs_path, h->buf, h->size, true)) {
                rc = -EIO;
            }
        } else {
            rc = -EROFS;
        }
    }

    if (h->buf) {
        kfree(h->buf);
        h->buf = NULL;
    }
    if (h->ents) {
        kfree(h->ents);
        h->ents = NULL;
    }
    kfree(h);
    return rc;
}

int32_t vfs_read(vfs_handle_t* h, void* dst, uint32_t len, uint32_t* out_read) {
    if (out_read) {
        *out_read = 0;
    }
    if (!h || !dst) {
        return -EINVAL;
    }
    if (h->kind != VFS_HANDLE_FILE) {
        return -EISDIR;
    }
    if (len == 0) {
        return 0;
    }

    uint32_t off = h->off;
    if (off >= h->size) {
        return 0;
    }

    uint32_t avail = h->size - off;
    uint32_t n = len;
    if (n > avail) {
        n = avail;
    }

    const uint8_t* src = h->ro_data ? h->ro_data : h->buf;
    if (!src && n != 0) {
        return -EIO;
    }

    memcpy(dst, src + off, n);
    h->off = off + n;
    if (out_read) {
        *out_read = n;
    }
    return 0;
}

int32_t vfs_write(vfs_handle_t* h, const void* src, uint32_t len, uint32_t* out_written) {
    if (out_written) {
        *out_written = 0;
    }
    if (!h || (!src && len != 0)) {
        return -EINVAL;
    }
    if (h->kind != VFS_HANDLE_FILE) {
        return -EISDIR;
    }
    if (!handle_writable(h)) {
        return -EBADF;
    }
    if (h->backend == VFS_BACKEND_INITRAMFS) {
        return -EROFS;
    }
    if (len == 0) {
        return 0;
    }

    if ((h->flags & VFS_O_APPEND) != 0) {
        h->off = h->size;
    }

    int32_t rc = handle_ensure_buf(h);
    if (rc < 0) {
        return rc;
    }

    uint32_t off = h->off;
    uint32_t end = off + len;
    if (end < off) {
        return -EOVERFLOW;
    }

    rc = handle_grow(h, end);
    if (rc < 0) {
        return rc;
    }

    // Zero-fill any gap.
    if (off > h->size) {
        memset(h->buf + h->size, 0, (size_t)(off - h->size));
    }

    memcpy(h->buf + off, src, len);
    h->off = off + len;
    if (end > h->size) {
        h->size = end;
    }
    h->dirty = true;
    if (out_written) {
        *out_written = len;
    }
    return 0;
}

int32_t vfs_lseek(vfs_handle_t* h, int32_t offset, int32_t whence, uint32_t* out_new_off) {
    if (out_new_off) {
        *out_new_off = 0;
    }
    if (!h) {
        return -EINVAL;
    }
    if (h->kind != VFS_HANDLE_FILE) {
        return -ESPIPE;
    }

    int64_t base = 0;
    if (whence == VFS_SEEK_SET) {
        base = 0;
    } else if (whence == VFS_SEEK_CUR) {
        base = (int64_t)h->off;
    } else if (whence == VFS_SEEK_END) {
        base = (int64_t)h->size;
    } else {
        return -EINVAL;
    }

    int64_t pos = base + (int64_t)offset;
    if (pos < 0) {
        return -EINVAL;
    }
    if (pos > 0x7FFFFFFF) {
        return -EOVERFLOW;
    }

    h->off = (uint32_t)pos;
    if (out_new_off) {
        *out_new_off = h->off;
    }
    return 0;
}

int32_t vfs_fstat(vfs_handle_t* h, vfs_stat_t* out) {
    if (!h || !out) {
        return -EINVAL;
    }
    memset(out, 0, sizeof(*out));
    out->is_dir = (h->kind == VFS_HANDLE_DIR) ? 1u : 0u;
    out->is_symlink = 0;
    out->mode = out->is_dir ? 0755 : 0644;
    out->size = (h->kind == VFS_HANDLE_FILE) ? h->size : 0;

    uint16_t wtime = 0;
    uint16_t wdate = 0;

    if (h->backend == VFS_BACKEND_MINIXFS) {
        const char* rel = h->abs_path + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";
        minixfs_stat_t mst;
        if (minixfs_stat(rel, &mst)) {
            out->mode = (uint16_t)(mst.mode & 07777u);
            out->uid = mst.uid;
            out->gid = mst.gid;
            unix_to_fat_ts(mst.mtime, &wtime, &wdate);
            out->wtime = wtime;
            out->wdate = wdate;
        }
    } else if (h->backend == VFS_BACKEND_RAMFS) {
        bool is_dir = false;
        uint32_t size = 0;
        if (ramfs_stat_ex(h->abs_path, &is_dir, &size, &wtime, &wdate)) {
            out->wtime = wtime;
            out->wdate = wdate;
        }
    } else {
        if (out->is_dir) {
            (void)initramfs_max_mtime_under_abs(h->abs_path, &wtime, &wdate);
        } else {
            (void)initramfs_lookup_mtime_abs(h->abs_path, &wtime, &wdate);
        }
        out->wtime = wtime;
        out->wdate = wdate;
    }
    return 0;
}

int32_t vfs_readdir(vfs_handle_t* h, vfs_dirent_t* out_ent) {
    if (!h || !out_ent) {
        return -EINVAL;
    }
    if (h->kind != VFS_HANDLE_DIR) {
        return -ENOTDIR;
    }
    if (h->ent_index >= h->ent_count) {
        return 0;
    }
    *out_ent = h->ents[h->ent_index++];
    return 1;
}

static void abs_dirname(const char* abs, char out[VFS_PATH_MAX]) {
    if (!out) {
        return;
    }
    if (!abs || abs[0] != '/') {
        out[0] = '/';
        out[1] = '\0';
        return;
    }

    strncpy(out, abs, VFS_PATH_MAX - 1u);
    out[VFS_PATH_MAX - 1u] = '\0';

    char* last = strrchr(out, '/');
    if (!last) {
        out[0] = '/';
        out[1] = '\0';
        return;
    }
    if (last == out) {
        out[1] = '\0';
        return;
    }
    *last = '\0';
}

int32_t vfs_unlink_path(const char* cwd, const char* path) {
    char abs[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve(cwd, path, abs);
    if (rc < 0) {
        return rc;
    }
    if (ci_eq(abs, "/") || ci_eq(abs, "/ram") || ci_eq(abs, "/disk") || ci_eq(abs, "/usr")) {
        return -EPERM;
    }

    char abs_usr[VFS_PATH_MAX];
    const char* aliased = abs_apply_posix_aliases(abs, abs_usr);

    char eff[VFS_PATH_MAX];
    rc = vfs_resolve_symlinks_abs(aliased, false, eff);
    if (rc < 0) {
        return rc;
    }

    if (abs_is_mount(eff, "/disk")) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* rel = eff + 5;  // skip "/disk"
        if (minixfs_is_dir(rel)) {
            return -EISDIR;
        }
        if (!minixfs_is_file(rel)) {
            return -ENOENT;
        }
        if (!minixfs_unlink(rel)) {
            return -EIO;
        }
        return 0;
    }

    if (abs_is_mount(eff, "/ram")) {
        if (ramfs_is_dir(eff)) {
            return -EISDIR;
        }
        if (!ramfs_is_file(eff)) {
            return -ENOENT;
        }
        if (!ramfs_unlink(eff)) {
            return -EIO;
        }
        return 0;
    }

    return -EROFS;
}

int32_t vfs_rmdir_path(const char* cwd, const char* path) {
    char abs[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve(cwd, path, abs);
    if (rc < 0) {
        return rc;
    }
    if (ci_eq(abs, "/") || ci_eq(abs, "/ram") || ci_eq(abs, "/disk") || ci_eq(abs, "/usr")) {
        return -EPERM;
    }

    char abs_usr[VFS_PATH_MAX];
    const char* aliased = abs_apply_posix_aliases(abs, abs_usr);

    char eff[VFS_PATH_MAX];
    rc = vfs_resolve_symlinks_abs(aliased, false, eff);
    if (rc < 0) {
        return rc;
    }

    if (abs_is_mount(eff, "/disk")) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* rel = eff + 5;  // skip "/disk"
        if (!minixfs_is_dir(rel)) {
            return minixfs_is_file(rel) ? -ENOTDIR : -ENOENT;
        }
        if (!minixfs_rmdir(rel)) {
            return -EIO;
        }
        return 0;
    }

    if (abs_is_mount(eff, "/ram")) {
        if (!ramfs_is_dir(eff)) {
            return ramfs_is_file(eff) ? -ENOTDIR : -ENOENT;
        }

        ramfs_dirent_t* dents = (ramfs_dirent_t*)kcalloc(VFS_MAX_DIR_ENTRIES, sizeof(ramfs_dirent_t));
        if (!dents) {
            return -ENOMEM;
        }
        uint32_t n = ramfs_list_dir(eff, dents, VFS_MAX_DIR_ENTRIES);
        kfree(dents);
        if (n != 0) {
            return -ENOTEMPTY;
        }

        if (!ramfs_rmdir(eff)) {
            return -EIO;
        }
        return 0;
    }

    return -EROFS;
}

int32_t vfs_rename_path(const char* cwd, const char* old_path, const char* new_path) {
    if (!old_path || !new_path) {
        return -EINVAL;
    }

    char abs_old[VFS_PATH_MAX];
    char abs_new[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve(cwd, old_path, abs_old);
    if (rc < 0) {
        return rc;
    }
    rc = vfs_path_resolve(cwd, new_path, abs_new);
    if (rc < 0) {
        return rc;
    }
    if (ci_eq(abs_old, "/") || ci_eq(abs_old, "/ram") || ci_eq(abs_old, "/disk") || ci_eq(abs_old, "/usr")) {
        return -EPERM;
    }
    if (ci_eq(abs_new, "/") || ci_eq(abs_new, "/ram") || ci_eq(abs_new, "/disk") || ci_eq(abs_new, "/usr")) {
        return -EPERM;
    }

    char abs_old_usr[VFS_PATH_MAX];
    char abs_new_usr[VFS_PATH_MAX];
    const char* old_alias = abs_apply_posix_aliases(abs_old, abs_old_usr);
    const char* new_alias = abs_apply_posix_aliases(abs_new, abs_new_usr);

    char old_eff[VFS_PATH_MAX];
    rc = vfs_resolve_symlinks_abs(old_alias, false, old_eff);
    if (rc < 0) {
        return rc;
    }

    char new_eff_check[VFS_PATH_MAX];
    rc = vfs_resolve_symlinks_abs(new_alias, false, new_eff_check);
    if (rc == 0) {
        return -EEXIST;
    }
    if (rc != -ENOENT) {
        return rc;
    }

    const char* base = strrchr(new_alias, '/');
    base = base ? base + 1 : new_alias;
    if (!base || base[0] == '\0') {
        return -EINVAL;
    }

    char parent[VFS_PATH_MAX];
    abs_dirname(new_alias, parent);

    char parent_res[VFS_PATH_MAX];
    rc = vfs_resolve_symlinks_abs(parent, true, parent_res);
    if (rc < 0) {
        return rc;
    }

    vfs_stat_t pst;
    rc = vfs_lstat_abs(parent_res, &pst);
    if (rc < 0) {
        return rc;
    }
    if (!pst.is_dir) {
        return -ENOTDIR;
    }

    char new_eff[VFS_PATH_MAX];
    size_t plen = strlen(parent_res);
    size_t blen = strlen(base);
    size_t need = plen + ((plen > 1u) ? 1u : 0u) + blen + 1u;
    if (need > VFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }
    size_t pos = 0;
    memcpy(new_eff + pos, parent_res, plen);
    pos += plen;
    if (plen > 1u) {
        new_eff[pos++] = '/';
    }
    memcpy(new_eff + pos, base, blen);
    pos += blen;
    new_eff[pos] = '\0';

    bool old_disk = abs_is_mount(old_eff, "/disk");
    bool old_ram = abs_is_mount(old_eff, "/ram");
    bool new_disk = abs_is_mount(new_eff, "/disk");
    bool new_ram = abs_is_mount(new_eff, "/ram");

    if ((old_disk && !new_disk) || (old_ram && !new_ram) ||
        (!old_disk && !old_ram) || (!new_disk && !new_ram)) {
        return -EXDEV;
    }

    if (old_disk) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        const char* old_rel = old_eff + 5;  // skip "/disk"
        const char* new_rel = new_eff + 5;
        if (!minixfs_rename(old_rel, new_rel)) {
            return -EIO;
        }
        return 0;
    }

    // /ram
    if (!ramfs_rename(old_eff, new_eff)) {
        return -EIO;
    }
    return 0;
}

int32_t vfs_ftruncate(vfs_handle_t* h, uint32_t new_size) {
    if (!h) {
        return -EINVAL;
    }
    if (h->kind != VFS_HANDLE_FILE) {
        return -EISDIR;
    }
    if (!handle_writable(h)) {
        return -EBADF;
    }
    if (h->backend == VFS_BACKEND_INITRAMFS) {
        return -EROFS;
    }

    int32_t rc = handle_ensure_buf(h);
    if (rc < 0) {
        return rc;
    }

    if (new_size > h->cap) {
        rc = handle_grow(h, new_size);
        if (rc < 0) {
            return rc;
        }
    }

    if (new_size > h->size) {
        memset(h->buf + h->size, 0, (size_t)(new_size - h->size));
    }

    h->size = new_size;
    if (h->off > new_size) {
        h->off = new_size;
    }
    h->dirty = true;
    return 0;
}

int32_t vfs_fsync(vfs_handle_t* h) {
    if (!h) {
        return -EINVAL;
    }
    if (h->kind != VFS_HANDLE_FILE) {
        return 0;
    }
    if (!handle_writable(h)) {
        return -EBADF;
    }
    if (h->backend == VFS_BACKEND_INITRAMFS) {
        return -EROFS;
    }
    if (!h->dirty) {
        return 0;
    }

    int32_t rc = 0;
    if (h->backend == VFS_BACKEND_MINIXFS) {
        const char* rel = h->abs_path + 5;  // skip "/disk"
        if (*rel == '\0') rel = "/";
        if (!minixfs_write_file(rel, h->buf, h->size)) {
            rc = -EIO;
        }
    } else if (h->backend == VFS_BACKEND_RAMFS) {
        if (!ramfs_write_file(h->abs_path, h->buf, h->size, true)) {
            rc = -EIO;
        }
    } else {
        rc = -EROFS;
    }

    if (rc == 0) {
        h->dirty = false;
    }
    return rc;
}

int32_t vfs_fchmod(vfs_handle_t* h, uint16_t mode) {
    if (!h) {
        return -EINVAL;
    }
    if (h->backend == VFS_BACKEND_INITRAMFS) {
        return -EROFS;
    }

    vfs_stat_t st;
    int32_t rc = vfs_lstat_abs(h->abs_path, &st);
    if (rc < 0) {
        return rc;
    }

    mode &= 07777u;

    if (h->backend == VFS_BACKEND_MINIXFS) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        if (!minixfs_chmod(h->abs_path + 5, mode)) {  // skip "/disk"
            return -EIO;
        }
        return 0;
    }

    if (h->backend == VFS_BACKEND_RAMFS) {
        if (!ramfs_set_meta(h->abs_path, false, mode)) {
            return -EIO;
        }
        return 0;
    }

    return -EROFS;
}

int32_t vfs_fchown(vfs_handle_t* h, uint32_t uid, uint32_t gid) {
    if (!h) {
        return -EINVAL;
    }
    if (h->backend == VFS_BACKEND_INITRAMFS) {
        return -EROFS;
    }

    if (h->backend == VFS_BACKEND_MINIXFS) {
        if (!minixfs_is_ready()) {
            return -EIO;
        }
        if (!minixfs_chown(h->abs_path + 5, (uint16_t)uid, (uint16_t)gid)) {  // skip "/disk"
            return -EIO;
        }
        return 0;
    }

    if (h->backend == VFS_BACKEND_RAMFS) {
        if (!ramfs_set_owner(h->abs_path, uid, gid)) {
            return -EIO;
        }
        return 0;
    }

    return -EROFS;
}

int32_t vfs_truncate_path(const char* cwd, const char* path, uint32_t new_size) {
    if (!path) {
        return -EINVAL;
    }

    vfs_handle_t* h = NULL;
    int32_t rc = vfs_open_path(cwd, path, VFS_O_RDWR, &h);
    if (rc < 0) {
        return rc;
    }
    if (!h) {
        return -EIO;
    }

    rc = vfs_ftruncate(h, new_size);
    int32_t rc_close = vfs_close(h);
    if (rc == 0 && rc_close < 0) {
        rc = rc_close;
    }
    return rc;
}

uint32_t vfs_handle_flags(vfs_handle_t* h) {
    if (!h) {
        return 0;
    }
    return h->flags;
}

int32_t vfs_handle_set_flags(vfs_handle_t* h, uint32_t flags) {
    if (!h) {
        return -EINVAL;
    }
    h->flags = flags;
    return 0;
}
