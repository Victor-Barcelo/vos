#include "vfs.h"

#include "ctype.h"
#include "fatdisk.h"
#include "kerrno.h"
#include "kheap.h"
#include "ramfs.h"
#include "string.h"

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
    VFS_BACKEND_FATDISK = 2,
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
        out->size = 0;
        out->wtime = 0;
        out->wdate = 0;
        return 0;
    }

    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (vfs_read_file(abs_path, &data, &size) && data) {
        out->is_dir = 0;
        out->size = size;
        (void)initramfs_lookup_mtime_abs(abs_path, &out->wtime, &out->wdate);
        return 0;
    }

    // Directory: any file under this prefix.
    const char* rel = abs_path;
    while (*rel == '/') rel++;
    if (*rel == '\0') {
        out->is_dir = 1;
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
            out->size = 0;
            (void)initramfs_max_mtime_under_abs(abs_path, &out->wtime, &out->wdate);
            return 0;
        }
    }

    // Mountpoints exposed at root even if initramfs doesn't have them.
    if (ci_eq(abs_path, "/ram")) {
        out->is_dir = 1;
        out->size = 0;
        out->wtime = 0;
        out->wdate = 0;
        return 0;
    }
    if (ci_eq(abs_path, "/disk")) {
        out->is_dir = 1;
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
        count = add_unique_dirent(out, count, max, "disk", true, 0, 0, 0);
        count = add_unique_dirent(out, count, max, "ram", true, 0, 0, 0);
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

int32_t vfs_stat_path(const char* cwd, const char* path, vfs_stat_t* out) {
    if (!out) {
        return -EINVAL;
    }

    char abs[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve(cwd, path, abs);
    if (rc < 0) {
        return rc;
    }

    if (abs_is_mount(abs, "/disk")) {
        if (!fatdisk_is_ready()) {
            return -EIO;
        }
        bool is_dir = false;
        uint32_t size = 0;
        uint16_t wtime = 0;
        uint16_t wdate = 0;
        if (!fatdisk_stat_ex(abs, &is_dir, &size, &wtime, &wdate)) {
            return -ENOENT;
        }
        out->is_dir = is_dir ? 1u : 0u;
        out->size = size;
        out->wtime = wtime;
        out->wdate = wdate;
        return 0;
    }

    if (abs_is_mount(abs, "/ram")) {
        bool is_dir = false;
        uint32_t size = 0;
        uint16_t wtime = 0;
        uint16_t wdate = 0;
        if (!ramfs_stat_ex(abs, &is_dir, &size, &wtime, &wdate)) {
            return -ENOENT;
        }
        out->is_dir = is_dir ? 1u : 0u;
        out->size = size;
        out->wtime = wtime;
        out->wdate = wdate;
        return 0;
    }

    return initramfs_stat_abs(abs, out);
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

    if (abs_is_mount(abs, "/disk")) {
        if (!fatdisk_is_ready()) {
            return -EIO;
        }
        bool is_dir = false;
        uint32_t size = 0;
        if (fatdisk_stat(abs, &is_dir, &size)) {
            return -EEXIST;
        }
        if (!fatdisk_mkdir(abs)) {
            return -EIO;
        }
        return 0;
    }

    if (abs_is_mount(abs, "/ram")) {
        if (ramfs_is_dir(abs) || ramfs_is_file(abs)) {
            return -EEXIST;
        }
        if (!ramfs_mkdir(abs)) {
            return -EIO;
        }
        return 0;
    }

    // initramfs is read-only.
    return -EROFS;
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

    if (backend == VFS_BACKEND_FATDISK) {
        fatdisk_dirent_t* dents = (fatdisk_dirent_t*)kcalloc(VFS_MAX_DIR_ENTRIES, sizeof(fatdisk_dirent_t));
        if (!dents) {
            kfree(h);
            return -ENOMEM;
        }
        count = fatdisk_list_dir(abs_path, dents, VFS_MAX_DIR_ENTRIES);
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
                h->ents[i].size = dents[i].size;
                h->ents[i].wtime = dents[i].wtime;
                h->ents[i].wdate = dents[i].wdate;
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

    char abs[VFS_PATH_MAX];
    int32_t rc = vfs_path_resolve(cwd, path, abs);
    if (rc < 0) {
        return rc;
    }

    uint32_t acc = flags & VFS_O_ACCMODE;
    bool want_write = (acc == VFS_O_WRONLY || acc == VFS_O_RDWR);
    bool want_dir = (flags & VFS_O_DIRECTORY) != 0;

    // /disk
    if (abs_is_mount(abs, "/disk")) {
        if (!fatdisk_is_ready()) {
            return -EIO;
        }

        bool is_dir = false;
        uint32_t size = 0;
        bool exists = fatdisk_stat(abs, &is_dir, &size);

        if (exists) {
            if (want_dir && !is_dir) {
                return -ENOTDIR;
            }
            if (is_dir) {
                if (want_write) {
                    return -EISDIR;
                }
                return open_dir_handle(VFS_BACKEND_FATDISK, abs, flags, out);
            }
            if ((flags & VFS_O_CREAT) && (flags & VFS_O_EXCL)) {
                return -EEXIST;
            }
            if (want_write && (flags & VFS_O_TRUNC)) {
                if (!fatdisk_write_file(abs, NULL, 0, true)) {
                    return -EIO;
                }
                size = 0;
            }
        } else {
            if (want_dir) {
                return -ENOENT;
            }
            if ((flags & VFS_O_CREAT) == 0) {
                return -ENOENT;
            }
            // Create empty file.
            if (!fatdisk_write_file(abs, NULL, 0, false)) {
                return -EIO;
            }
            size = 0;
        }

        uint8_t* data = NULL;
        if (!want_write && (flags & VFS_O_TRUNC) == 0) {
            // Read-only: keep an owned buffer to simplify lifetime.
            if (size) {
                if (!fatdisk_read_file_alloc(abs, &data, &size) || !data) {
                    return -EIO;
                }
            } else {
                data = NULL;
            }
        } else if ((flags & VFS_O_TRUNC) == 0) {
            if (size) {
                if (!fatdisk_read_file_alloc(abs, &data, &size) || !data) {
                    return -EIO;
                }
            }
        }

        rc = open_file_handle(VFS_BACKEND_FATDISK, abs, flags, data, size, out);
        if (rc < 0) {
            if (data) kfree(data);
            return rc;
        }
        // For FAT-backed files we always treat `data` as owned (it came from kmalloc).
        (*out)->buf = (uint8_t*)data;
        (*out)->cap = size;
        (*out)->ro_data = (*out)->buf;
        return 0;
    }

    // /ram
    if (abs_is_mount(abs, "/ram")) {
        bool is_dir = ramfs_is_dir(abs);
        bool is_file = ramfs_is_file(abs);

        if (is_dir || is_file) {
            if (want_dir && !is_dir) {
                return -ENOTDIR;
            }
            if (is_dir) {
                if (want_write) {
                    return -EISDIR;
                }
                return open_dir_handle(VFS_BACKEND_RAMFS, abs, flags, out);
            }
            if ((flags & VFS_O_CREAT) && (flags & VFS_O_EXCL)) {
                return -EEXIST;
            }
            if (want_write && (flags & VFS_O_TRUNC)) {
                if (!ramfs_write_file(abs, NULL, 0, true)) {
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
            if (!ramfs_write_file(abs, NULL, 0, false)) {
                return -EIO;
            }
        }

        const uint8_t* ro = NULL;
        uint32_t size = 0;
        if ((flags & VFS_O_TRUNC) == 0) {
            (void)ramfs_read_file(abs, &ro, &size);
        }

        return open_file_handle(VFS_BACKEND_RAMFS, abs, flags, ro, size, out);
    }

    // initramfs (read-only)
    if (want_write || (flags & VFS_O_CREAT)) {
        return -EROFS;
    }

    vfs_stat_t st;
    rc = initramfs_stat_abs(abs, &st);
    if (rc < 0) {
        return rc;
    }
    if (want_dir && !st.is_dir) {
        return -ENOTDIR;
    }
    if (st.is_dir) {
        return open_dir_handle(VFS_BACKEND_INITRAMFS, abs, flags, out);
    }

    const uint8_t* data = NULL;
    uint32_t size = 0;
    if (!vfs_read_file(abs, &data, &size) || !data) {
        return -ENOENT;
    }
    return open_file_handle(VFS_BACKEND_INITRAMFS, abs, flags, data, size, out);
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
        if (h->backend == VFS_BACKEND_FATDISK) {
            if (!fatdisk_write_file(h->abs_path, h->buf, h->size, true)) {
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
    out->size = (h->kind == VFS_HANDLE_FILE) ? h->size : 0;

    uint16_t wtime = 0;
    uint16_t wdate = 0;

    if (h->backend == VFS_BACKEND_FATDISK) {
        bool is_dir = false;
        uint32_t size = 0;
        if (fatdisk_stat_ex(h->abs_path, &is_dir, &size, &wtime, &wdate)) {
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
    if (ci_eq(abs, "/") || ci_eq(abs, "/ram") || ci_eq(abs, "/disk")) {
        return -EPERM;
    }

    if (abs_is_mount(abs, "/disk")) {
        if (!fatdisk_is_ready()) {
            return -EIO;
        }
        bool is_dir = false;
        uint32_t size = 0;
        if (!fatdisk_stat(abs, &is_dir, &size)) {
            return -ENOENT;
        }
        if (is_dir) {
            return -EISDIR;
        }
        if (!fatdisk_unlink(abs)) {
            return -EIO;
        }
        return 0;
    }

    if (abs_is_mount(abs, "/ram")) {
        if (ramfs_is_dir(abs)) {
            return -EISDIR;
        }
        if (!ramfs_is_file(abs)) {
            return -ENOENT;
        }
        if (!ramfs_unlink(abs)) {
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
    if (ci_eq(abs, "/") || ci_eq(abs, "/ram") || ci_eq(abs, "/disk")) {
        return -EPERM;
    }

    if (abs_is_mount(abs, "/disk")) {
        if (!fatdisk_is_ready()) {
            return -EIO;
        }
        bool is_dir = false;
        uint32_t size = 0;
        if (!fatdisk_stat(abs, &is_dir, &size)) {
            return -ENOENT;
        }
        if (!is_dir) {
            return -ENOTDIR;
        }

        fatdisk_dirent_t* dents = (fatdisk_dirent_t*)kcalloc(VFS_MAX_DIR_ENTRIES, sizeof(fatdisk_dirent_t));
        if (!dents) {
            return -ENOMEM;
        }
        uint32_t n = fatdisk_list_dir(abs, dents, VFS_MAX_DIR_ENTRIES);
        for (uint32_t i = 0; i < n; i++) {
            if (ci_eq(dents[i].name, ".") || ci_eq(dents[i].name, "..")) {
                continue;
            }
            kfree(dents);
            return -ENOTEMPTY;
        }
        kfree(dents);

        if (!fatdisk_rmdir(abs)) {
            return -EIO;
        }
        return 0;
    }

    if (abs_is_mount(abs, "/ram")) {
        if (!ramfs_is_dir(abs)) {
            return ramfs_is_file(abs) ? -ENOTDIR : -ENOENT;
        }

        ramfs_dirent_t* dents = (ramfs_dirent_t*)kcalloc(VFS_MAX_DIR_ENTRIES, sizeof(ramfs_dirent_t));
        if (!dents) {
            return -ENOMEM;
        }
        uint32_t n = ramfs_list_dir(abs, dents, VFS_MAX_DIR_ENTRIES);
        kfree(dents);
        if (n != 0) {
            return -ENOTEMPTY;
        }

        if (!ramfs_rmdir(abs)) {
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
    if (ci_eq(abs_old, "/") || ci_eq(abs_old, "/ram") || ci_eq(abs_old, "/disk")) {
        return -EPERM;
    }
    if (ci_eq(abs_new, "/") || ci_eq(abs_new, "/ram") || ci_eq(abs_new, "/disk")) {
        return -EPERM;
    }

    vfs_stat_t st_old;
    rc = vfs_stat_path("/", abs_old, &st_old);
    if (rc < 0) {
        return rc;
    }

    vfs_stat_t st_new;
    rc = vfs_stat_path("/", abs_new, &st_new);
    if (rc == 0) {
        return -EEXIST;
    }
    if (rc != -ENOENT) {
        return rc;
    }

    bool old_disk = abs_is_mount(abs_old, "/disk");
    bool old_ram = abs_is_mount(abs_old, "/ram");
    bool new_disk = abs_is_mount(abs_new, "/disk");
    bool new_ram = abs_is_mount(abs_new, "/ram");

    if ((old_disk && !new_disk) || (old_ram && !new_ram) || (!old_disk && !old_ram) || (!new_disk && !new_ram)) {
        return -EXDEV;
    }

    if (old_disk) {
        if (!fatdisk_is_ready()) {
            return -EIO;
        }

        // fatdisk_rename currently can't move across directories.
        char old_dir[VFS_PATH_MAX];
        char new_dir[VFS_PATH_MAX];
        abs_dirname(abs_old, old_dir);
        abs_dirname(abs_new, new_dir);
        if (!ci_eq(old_dir, new_dir)) {
            return -EXDEV;
        }

        if (!fatdisk_rename(abs_old, abs_new)) {
            return -EIO;
        }
        return 0;
    }

    // /ram
    if (!ramfs_rename(abs_old, abs_new)) {
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
    if (h->backend == VFS_BACKEND_FATDISK) {
        if (!fatdisk_write_file(h->abs_path, h->buf, h->size, true)) {
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
