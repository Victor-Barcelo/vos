#ifndef VFS_H
#define VFS_H

#include "types.h"
#include "multiboot.h"

void vfs_init(const multiboot_info_t* mbi);
bool vfs_is_ready(void);

uint32_t vfs_file_count(void);
const char* vfs_file_name(uint32_t idx);
uint32_t vfs_file_size(uint32_t idx);
bool vfs_file_mtime(uint32_t idx, uint16_t* out_wtime, uint16_t* out_wdate);

bool vfs_read_file(const char* path, const uint8_t** out_data, uint32_t* out_size);

// -----------------------------
// Mount-aware VFS (POSIX-ish)
// -----------------------------

// Canonical absolute path length limit (including NUL).
#define VFS_PATH_MAX 256u

// Directory entry name limit (including NUL).
#define VFS_NAME_MAX 64u

// Maximum number of entries returned by a single directory listing.
#define VFS_MAX_DIR_ENTRIES 256u

typedef struct vfs_stat {
    bool is_dir;
    uint32_t size;
} vfs_stat_t;

typedef struct vfs_dirent {
    char name[VFS_NAME_MAX];
    bool is_dir;
    uint32_t size;
} vfs_dirent_t;

typedef struct vfs_handle vfs_handle_t;

// Resolve `path` relative to `cwd` into a canonical absolute path.
// Returns 0 on success, or -errno on failure.
int32_t vfs_path_resolve(const char* cwd, const char* path, char out_abs[VFS_PATH_MAX]);

// Path-based helpers (cwd + path). Returns 0 on success, or -errno.
int32_t vfs_stat_path(const char* cwd, const char* path, vfs_stat_t* out);
int32_t vfs_mkdir_path(const char* cwd, const char* path);

// Open/close/read/write/lseek on a VFS handle. Returns 0 on success, or -errno.
int32_t vfs_open_path(const char* cwd, const char* path, uint32_t flags, vfs_handle_t** out);
void vfs_ref(vfs_handle_t* h);
int32_t vfs_close(vfs_handle_t* h);
int32_t vfs_read(vfs_handle_t* h, void* dst, uint32_t len, uint32_t* out_read);
int32_t vfs_write(vfs_handle_t* h, const void* src, uint32_t len, uint32_t* out_written);
int32_t vfs_lseek(vfs_handle_t* h, int32_t offset, int32_t whence, uint32_t* out_new_off);
int32_t vfs_fstat(vfs_handle_t* h, vfs_stat_t* out);

// Read the next directory entry from an opened directory handle.
// Returns 1 if an entry was written to `out_ent`, 0 at end-of-directory, or -errno.
int32_t vfs_readdir(vfs_handle_t* h, vfs_dirent_t* out_ent);

// Additional POSIX-ish helpers.
int32_t vfs_unlink_path(const char* cwd, const char* path);
int32_t vfs_rmdir_path(const char* cwd, const char* path);
int32_t vfs_rename_path(const char* cwd, const char* old_path, const char* new_path);
int32_t vfs_truncate_path(const char* cwd, const char* path, uint32_t new_size);
int32_t vfs_ftruncate(vfs_handle_t* h, uint32_t new_size);
int32_t vfs_fsync(vfs_handle_t* h);

#endif
