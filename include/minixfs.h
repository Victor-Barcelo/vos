// Minix filesystem driver (v1 and v2)
#ifndef MINIXFS_H
#define MINIXFS_H

#include "types.h"

// Minix filesystem magic numbers
#define MINIX_SUPER_MAGIC   0x137F  // Minix v1, 14-char names
#define MINIX_SUPER_MAGIC2  0x138F  // Minix v1, 30-char names
#define MINIX2_SUPER_MAGIC  0x2468  // Minix v2, 14-char names
#define MINIX2_SUPER_MAGIC2 0x2478  // Minix v2, 30-char names

// Block and inode sizes
#define MINIX_BLOCK_SIZE    1024
#define MINIX_INODE_SIZE_V1 32
#define MINIX_INODE_SIZE_V2 64

// Maximum name lengths
#define MINIX_NAME_LEN_14   14
#define MINIX_NAME_LEN_30   30

// File types (in mode)
#define MINIX_S_IFMT   0170000  // File type mask
#define MINIX_S_IFREG  0100000  // Regular file
#define MINIX_S_IFDIR  0040000  // Directory
#define MINIX_S_IFLNK  0120000  // Symbolic link
#define MINIX_S_IFBLK  0060000  // Block device
#define MINIX_S_IFCHR  0020000  // Character device
#define MINIX_S_IFIFO  0010000  // FIFO

// Permission bits
#define MINIX_S_ISUID  0004000
#define MINIX_S_ISGID  0002000
#define MINIX_S_ISVTX  0001000
#define MINIX_S_IRWXU  0000700
#define MINIX_S_IRUSR  0000400
#define MINIX_S_IWUSR  0000200
#define MINIX_S_IXUSR  0000100
#define MINIX_S_IRWXG  0000070
#define MINIX_S_IRGRP  0000040
#define MINIX_S_IWGRP  0000020
#define MINIX_S_IXGRP  0000010
#define MINIX_S_IRWXO  0000007
#define MINIX_S_IROTH  0000004
#define MINIX_S_IWOTH  0000002
#define MINIX_S_IXOTH  0000001

// Type check macros
#define MINIX_S_ISREG(m)  (((m) & MINIX_S_IFMT) == MINIX_S_IFREG)
#define MINIX_S_ISDIR(m)  (((m) & MINIX_S_IFMT) == MINIX_S_IFDIR)
#define MINIX_S_ISLNK(m)  (((m) & MINIX_S_IFMT) == MINIX_S_IFLNK)

// Root inode number
#define MINIX_ROOT_INO  1

// Minix v1 superblock (on-disk)
typedef struct __attribute__((packed)) {
    uint16_t s_ninodes;         // Number of inodes
    uint16_t s_nzones;          // Number of zones (blocks)
    uint16_t s_imap_blocks;     // Inode bitmap blocks
    uint16_t s_zmap_blocks;     // Zone bitmap blocks
    uint16_t s_firstdatazone;   // First data zone
    uint16_t s_log_zone_size;   // Log2(zone size / block size)
    uint32_t s_max_size;        // Maximum file size
    uint16_t s_magic;           // Magic number
    uint16_t s_state;           // Filesystem state
} minix_super_block_v1_t;

// Minix v2 superblock (on-disk)
typedef struct __attribute__((packed)) {
    uint16_t s_ninodes;         // Number of inodes
    uint16_t s_nzones;          // Number of zones (v1 compat)
    uint16_t s_imap_blocks;     // Inode bitmap blocks
    uint16_t s_zmap_blocks;     // Zone bitmap blocks
    uint16_t s_firstdatazone;   // First data zone
    uint16_t s_log_zone_size;   // Log2(zone size / block size)
    uint32_t s_max_size;        // Maximum file size
    uint16_t s_magic;           // Magic number
    uint16_t s_state;           // Filesystem state
    uint32_t s_zones;           // Total zones (v2)
} minix_super_block_v2_t;

// Minix v1 inode (on-disk, 32 bytes)
typedef struct __attribute__((packed)) {
    uint16_t i_mode;            // File type and permissions
    uint16_t i_uid;             // Owner user ID
    uint32_t i_size;            // File size in bytes
    uint32_t i_time;            // Modification time
    uint8_t  i_gid;             // Owner group ID
    uint8_t  i_nlinks;          // Number of hard links
    uint16_t i_zone[9];         // Zone numbers: 0-6 direct, 7 indirect, 8 double indirect
} minix_inode_v1_t;

// Minix v2 inode (on-disk, 64 bytes)
typedef struct __attribute__((packed)) {
    uint16_t i_mode;            // File type and permissions
    uint16_t i_nlinks;          // Number of hard links
    uint16_t i_uid;             // Owner user ID
    uint16_t i_gid;             // Owner group ID
    uint32_t i_size;            // File size in bytes
    uint32_t i_atime;           // Access time
    uint32_t i_mtime;           // Modification time
    uint32_t i_ctime;           // Change time
    uint32_t i_zone[10];        // Zone numbers: 0-6 direct, 7 indirect, 8 double, 9 triple
} minix_inode_v2_t;

// Directory entry (v1, 14-char names)
typedef struct __attribute__((packed)) {
    uint16_t inode;
    char name[14];
} minix_dir_entry_14_t;

// Directory entry (v2, 30-char names)
typedef struct __attribute__((packed)) {
    uint16_t inode;
    char name[30];
} minix_dir_entry_30_t;

// Stat structure for minixfs
typedef struct {
    uint16_t mode;              // File type and permissions
    uint16_t uid;               // Owner user ID
    uint16_t gid;               // Owner group ID
    uint32_t size;              // File size
    uint32_t mtime;             // Modification time
    uint16_t nlinks;            // Number of hard links
    uint32_t ino;               // Inode number
} minixfs_stat_t;

// Directory entry for listing
typedef struct {
    uint32_t inode;
    char name[31];              // Max 30 chars + null
    bool is_dir;
} minixfs_dirent_t;

// Initialize minixfs on a partition
// partition_lba_start: starting LBA of the partition
// Returns true on success
bool minixfs_init(uint32_t partition_lba_start);

// Check if minixfs is mounted
bool minixfs_is_ready(void);

// Get filesystem info
bool minixfs_statfs(uint32_t* total_blocks, uint32_t* free_blocks, uint32_t* total_inodes, uint32_t* free_inodes);

// Stat a file/directory by path (relative to minixfs root)
bool minixfs_stat(const char* path, minixfs_stat_t* out);

// Check if path is a directory
bool minixfs_is_dir(const char* path);

// Check if path is a file
bool minixfs_is_file(const char* path);

// Read file contents
// Returns allocated buffer (caller must free with kfree), sets *out_size
uint8_t* minixfs_read_file(const char* path, uint32_t* out_size);

// Write file contents (creates or overwrites)
bool minixfs_write_file(const char* path, const uint8_t* data, uint32_t size);

// Create directory
bool minixfs_mkdir(const char* path);

// Remove file
bool minixfs_unlink(const char* path);

// Remove directory (must be empty)
bool minixfs_rmdir(const char* path);

// List directory contents
// Returns number of entries, fills out array up to max_entries
uint32_t minixfs_readdir(const char* path, minixfs_dirent_t* out, uint32_t max_entries);

// Read symbolic link target
bool minixfs_readlink(const char* path, char* buf, uint32_t bufsize);

// Create symbolic link
bool minixfs_symlink(const char* target, const char* linkpath);

// Change file permissions
bool minixfs_chmod(const char* path, uint16_t mode);

// Change file owner
bool minixfs_chown(const char* path, uint16_t uid, uint16_t gid);

// Rename/move file
bool minixfs_rename(const char* oldpath, const char* newpath);

// Sync filesystem (flush caches)
void minixfs_sync(void);

#endif // MINIXFS_H
