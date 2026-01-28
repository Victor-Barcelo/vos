# Chapter 18: Minix Filesystem

VOS uses the Minix filesystem (version 2 with 30-character filenames) for persistent storage on the ATA disk. Minix is a simple, reliable Unix-style filesystem that provides proper inode-based storage with directory hierarchies.

## Why Minix?

1. **Unix semantics** - Proper inodes, hard links, permissions
2. **Simple implementation** - Well-documented, educational
3. **Long filenames** - 30-character names (v2) or 60-character (v3)
4. **Linux compatibility** - Can be created with `mkfs.minix` and mounted on Linux
5. **Sufficient capacity** - Supports volumes up to 4GB with v2

## Minix Filesystem Structure

### Superblock

The superblock is located at offset 1024 bytes (block 1 for 1KB blocks):

```c
typedef struct {
    uint16_t s_ninodes;        // Number of inodes
    uint16_t s_nzones;         // Number of zones (v1) or unused (v2)
    uint16_t s_imap_blocks;    // Inode bitmap blocks
    uint16_t s_zmap_blocks;    // Zone bitmap blocks
    uint16_t s_firstdatazone;  // First data zone
    uint16_t s_log_zone_size;  // Log2(zones/block)
    uint32_t s_max_size;       // Maximum file size
    uint16_t s_magic;          // Magic number
    uint16_t s_state;          // Mount state
    uint32_t s_zones;          // Total zones (v2)
} __attribute__((packed)) minix_superblock_t;

// Magic numbers
#define MINIX_SUPER_MAGIC   0x137F  // V1 (14-char names)
#define MINIX_SUPER_MAGIC2  0x138F  // V1 (30-char names)
#define MINIX2_SUPER_MAGIC  0x2468  // V2 (14-char names)
#define MINIX2_SUPER_MAGIC2 0x2478  // V2 (30-char names) <- VOS uses this
```

### Disk Layout

```
+------------------+
| Boot Block       | Block 0 (1024 bytes, unused by fs)
+------------------+
| Superblock       | Block 1
+------------------+
| Inode Bitmap     | Blocks 2 to 2+s_imap_blocks-1
+------------------+
| Zone Bitmap      | Following inode bitmap
+------------------+
| Inode Table      | Fixed-size inode array
+------------------+
| Data Zones       | File and directory content
+------------------+
```

### Inode Structure (v2)

```c
typedef struct {
    uint16_t i_mode;       // File type and permissions
    uint16_t i_nlinks;     // Number of hard links
    uint16_t i_uid;        // Owner user ID
    uint16_t i_gid;        // Owner group ID
    uint32_t i_size;       // File size in bytes
    uint32_t i_atime;      // Access time
    uint32_t i_mtime;      // Modification time
    uint32_t i_ctime;      // Status change time
    uint32_t i_zone[10];   // Zone pointers:
                           //   [0-6]  Direct zones
                           //   [7]    Single indirect
                           //   [8]    Double indirect
                           //   [9]    Triple indirect (v2)
} __attribute__((packed)) minix2_inode_t;

// File type masks (same as POSIX)
#define S_IFMT   0170000   // Type mask
#define S_IFREG  0100000   // Regular file
#define S_IFDIR  0040000   // Directory
#define S_IFCHR  0020000   // Character device
#define S_IFBLK  0060000   // Block device
#define S_IFLNK  0120000   // Symbolic link
```

### Directory Entry (30-char names)

```c
typedef struct {
    uint32_t inode;        // Inode number (0 = deleted)
    char     name[30];     // Filename (null-padded)
} __attribute__((packed)) minix_dirent30_t;
```

## VOS Minix Implementation

### Initialization

```c
// kernel/minixfs.c
static minix_fs_t g_minix;

bool minixfs_init(uint32_t partition_lba) {
    g_minix.partition_lba = partition_lba;

    // Read superblock (at offset 1024 from partition start)
    uint8_t buf[1024];
    if (!ata_read_sectors(partition_lba + 2, 2, buf)) {
        return false;
    }

    minix_superblock_t* sb = (minix_superblock_t*)buf;

    // Check magic number
    if (sb->s_magic != MINIX2_SUPER_MAGIC2) {
        serial_printf("[MINIXFS] Unknown magic: 0x%x\n", sb->s_magic);
        return false;
    }

    // Store filesystem parameters
    g_minix.ninodes = sb->s_ninodes;
    g_minix.nzones = sb->s_zones;
    g_minix.imap_blocks = sb->s_imap_blocks;
    g_minix.zmap_blocks = sb->s_zmap_blocks;
    g_minix.firstdatazone = sb->s_firstdatazone;
    g_minix.block_size = 1024;  // Minix v2 uses 1KB blocks

    // Calculate layout
    g_minix.imap_start = 2;  // After boot + super
    g_minix.zmap_start = g_minix.imap_start + g_minix.imap_blocks;
    g_minix.inode_start = g_minix.zmap_start + g_minix.zmap_blocks;

    serial_printf("[MINIXFS] Mounted v2 (30-char names), "
                  "%u inodes, %u zones\n",
                  g_minix.ninodes, g_minix.nzones);

    g_minix.mounted = true;
    return true;
}
```

### Reading an Inode

```c
static bool minixfs_read_inode(uint32_t ino, minix2_inode_t* out) {
    if (ino < 1 || ino > g_minix.ninodes) {
        return false;
    }

    // Inodes are 1-indexed, 64 bytes each
    uint32_t inodes_per_block = g_minix.block_size / sizeof(minix2_inode_t);
    uint32_t block = g_minix.inode_start + (ino - 1) / inodes_per_block;
    uint32_t offset = ((ino - 1) % inodes_per_block) * sizeof(minix2_inode_t);

    uint8_t buf[1024];
    uint32_t lba = g_minix.partition_lba + block * 2;
    if (!ata_read_sectors(lba, 2, buf)) {
        return false;
    }

    memcpy(out, buf + offset, sizeof(minix2_inode_t));
    return true;
}
```

### Zone to LBA Conversion

```c
static uint32_t zone_to_lba(uint32_t zone) {
    // Zone 0 means "hole" (sparse file)
    if (zone == 0) return 0;
    return g_minix.partition_lba + zone * 2;  // 1KB blocks = 2 sectors
}
```

### Reading File Data

```c
static int32_t minixfs_read_zones(minix2_inode_t* inode,
                                   uint32_t offset, uint32_t size,
                                   uint8_t* buffer) {
    if (offset >= inode->i_size) return 0;
    if (offset + size > inode->i_size) {
        size = inode->i_size - offset;
    }

    uint32_t bytes_read = 0;
    uint8_t block_buf[1024];

    while (bytes_read < size) {
        uint32_t block_idx = (offset + bytes_read) / g_minix.block_size;
        uint32_t block_off = (offset + bytes_read) % g_minix.block_size;

        // Get zone number for this block
        uint32_t zone = minixfs_get_zone(inode, block_idx);

        if (zone == 0) {
            // Sparse file hole - return zeros
            uint32_t to_copy = g_minix.block_size - block_off;
            if (to_copy > size - bytes_read) {
                to_copy = size - bytes_read;
            }
            memset(buffer + bytes_read, 0, to_copy);
            bytes_read += to_copy;
            continue;
        }

        // Read the zone
        uint32_t lba = zone_to_lba(zone);
        if (!ata_read_sectors(lba, 2, block_buf)) {
            break;
        }

        uint32_t to_copy = g_minix.block_size - block_off;
        if (to_copy > size - bytes_read) {
            to_copy = size - bytes_read;
        }

        memcpy(buffer + bytes_read, block_buf + block_off, to_copy);
        bytes_read += to_copy;
    }

    return bytes_read;
}
```

### Zone Indirection

```c
static uint32_t minixfs_get_zone(minix2_inode_t* inode, uint32_t block_idx) {
    uint32_t ptrs_per_zone = g_minix.block_size / sizeof(uint32_t);  // 256

    // Direct zones (0-6)
    if (block_idx < 7) {
        return inode->i_zone[block_idx];
    }
    block_idx -= 7;

    // Single indirect (7)
    if (block_idx < ptrs_per_zone) {
        if (inode->i_zone[7] == 0) return 0;
        return read_indirect_zone(inode->i_zone[7], block_idx);
    }
    block_idx -= ptrs_per_zone;

    // Double indirect (8)
    if (block_idx < ptrs_per_zone * ptrs_per_zone) {
        if (inode->i_zone[8] == 0) return 0;
        uint32_t idx1 = block_idx / ptrs_per_zone;
        uint32_t idx2 = block_idx % ptrs_per_zone;
        uint32_t ind1 = read_indirect_zone(inode->i_zone[8], idx1);
        if (ind1 == 0) return 0;
        return read_indirect_zone(ind1, idx2);
    }

    // Triple indirect (9) - rarely needed
    // ... similar pattern ...

    return 0;
}
```

### Directory Lookup

```c
uint32_t minixfs_lookup(uint32_t dir_ino, const char* name) {
    minix2_inode_t dir;
    if (!minixfs_read_inode(dir_ino, &dir)) {
        return 0;
    }

    if (!S_ISDIR(dir.i_mode)) {
        return 0;
    }

    // Read directory entries
    uint8_t* data = kmalloc(dir.i_size);
    if (!data) return 0;

    minixfs_read_zones(&dir, 0, dir.i_size, data);

    uint32_t nentries = dir.i_size / sizeof(minix_dirent30_t);
    minix_dirent30_t* entries = (minix_dirent30_t*)data;

    for (uint32_t i = 0; i < nentries; i++) {
        if (entries[i].inode == 0) continue;  // Deleted

        if (strncmp(entries[i].name, name, 30) == 0) {
            uint32_t ino = entries[i].inode;
            kfree(data);
            return ino;
        }
    }

    kfree(data);
    return 0;
}
```

### Path Resolution

```c
uint32_t minixfs_path_to_inode(const char* path) {
    // Start from root inode (always 1 in Minix)
    uint32_t ino = MINIX_ROOT_INO;  // 1

    if (!path || path[0] != '/') {
        return 0;
    }
    path++;  // Skip leading '/'

    char component[31];

    while (*path) {
        // Skip slashes
        while (*path == '/') path++;
        if (!*path) break;

        // Extract component
        int i = 0;
        while (*path && *path != '/' && i < 30) {
            component[i++] = *path++;
        }
        component[i] = '\0';

        // Lookup in current directory
        ino = minixfs_lookup(ino, component);
        if (ino == 0) {
            return 0;  // Not found
        }
    }

    return ino;
}
```

## Writing Files

### Allocating a Zone

```c
static uint32_t minixfs_alloc_zone(void) {
    // Read zone bitmap
    uint8_t bitmap[1024];

    for (uint32_t blk = 0; blk < g_minix.zmap_blocks; blk++) {
        uint32_t lba = g_minix.partition_lba +
                       (g_minix.zmap_start + blk) * 2;
        ata_read_sectors(lba, 2, bitmap);

        for (int i = 0; i < 1024; i++) {
            if (bitmap[i] != 0xFF) {
                // Found a byte with free bit
                for (int bit = 0; bit < 8; bit++) {
                    if (!(bitmap[i] & (1 << bit))) {
                        // Mark as used
                        bitmap[i] |= (1 << bit);
                        ata_write_sectors(lba, 2, bitmap);

                        return blk * 8192 + i * 8 + bit;
                    }
                }
            }
        }
    }

    return 0;  // No free zones
}
```

### Creating a File

```c
uint32_t minixfs_create(uint32_t dir_ino, const char* name, uint16_t mode) {
    // 1. Allocate new inode
    uint32_t new_ino = minixfs_alloc_inode();
    if (new_ino == 0) return 0;

    // 2. Initialize inode
    minix2_inode_t inode = {0};
    inode.i_mode = mode;
    inode.i_nlinks = 1;
    inode.i_uid = 0;  // TODO: get from current task
    inode.i_gid = 0;
    inode.i_size = 0;
    inode.i_atime = inode.i_mtime = inode.i_ctime = get_unix_time();

    minixfs_write_inode(new_ino, &inode);

    // 3. Add directory entry
    minixfs_add_dirent(dir_ino, name, new_ino);

    return new_ino;
}
```

### Writing Data

```c
int32_t minixfs_write(uint32_t ino, uint32_t offset,
                      uint32_t size, const uint8_t* data) {
    minix2_inode_t inode;
    if (!minixfs_read_inode(ino, &inode)) {
        return -EIO;
    }

    uint32_t bytes_written = 0;
    uint8_t block_buf[1024];

    while (bytes_written < size) {
        uint32_t block_idx = (offset + bytes_written) / g_minix.block_size;
        uint32_t block_off = (offset + bytes_written) % g_minix.block_size;

        // Get or allocate zone
        uint32_t zone = minixfs_get_zone(&inode, block_idx);
        if (zone == 0) {
            zone = minixfs_alloc_zone();
            if (zone == 0) break;  // No space
            minixfs_set_zone(&inode, block_idx, zone);
        }

        // Read-modify-write for partial blocks
        uint32_t lba = zone_to_lba(zone);
        if (block_off != 0 || (size - bytes_written) < g_minix.block_size) {
            ata_read_sectors(lba, 2, block_buf);
        }

        uint32_t to_write = g_minix.block_size - block_off;
        if (to_write > size - bytes_written) {
            to_write = size - bytes_written;
        }

        memcpy(block_buf + block_off, data + bytes_written, to_write);
        ata_write_sectors(lba, 2, block_buf);

        bytes_written += to_write;
    }

    // Update size if extended
    if (offset + bytes_written > inode.i_size) {
        inode.i_size = offset + bytes_written;
    }
    inode.i_mtime = get_unix_time();
    minixfs_write_inode(ino, &inode);

    return bytes_written;
}
```

## VFS Integration

### Mount Point

VOS mounts the Minix partition at `/disk`:

```c
// In kernel/kernel.c
if (ata_is_present() && mbr_read()) {
    int minix_part = mbr_find_partition_by_type(MBR_TYPE_MINIX);
    if (minix_part >= 0) {
        const mbr_partition_t* p = mbr_get_partition(minix_part);
        if (p) {
            minixfs_init(p->lba_start);
        }
    }
}
```

### Path Routing

The VFS routes `/disk/*` paths to minixfs:

```c
// In kernel/vfs_posix.c
static vfs_backend_t identify_backend(const char* abs) {
    if (ci_starts_with(abs, "/disk")) {
        return VFS_BACKEND_MINIXFS;
    }
    // ... other backends ...
}
```

## First-Boot Initialization

When VOS detects a blank Minix disk, it offers to initialize it:

```
  ╔══════════════════════════════════════════════════════════════╗
  ║   ██╗   ██╗ ██████╗ ███████╗    First Boot Setup            ║
  ╚══════════════════════════════════════════════════════════════╝

  A blank disk has been detected.

  Options:
    [Y] Initialize disk for VOS
        - Creates /bin, /etc, /home directories
        - Sets up root and victor users
        - Copies system binaries
        - All changes will persist across reboots

    [N] Boot in Live Mode
        - No changes written to disk
        - All data lost on reboot
        - Good for testing

  Initialize disk for VOS? [Y/n]
```

## Creating a Minix Disk Image

```bash
# Create 4GB disk image with MBR (default VOS size)
dd if=/dev/zero of=vos-disk.img bs=1M count=4096

# Create partition table (Minix partition starting at sector 2048)
echo -e "o\nn\np\n1\n2048\n\nt\n81\nw" | fdisk vos-disk.img

# Create Minix filesystem on the partition
# Extract partition, format, replace
dd if=vos-disk.img of=part.img bs=512 skip=2048
mkfs.minix -2 -n 30 part.img
dd if=part.img of=vos-disk.img bs=512 seek=2048 conv=notrunc
rm part.img
```

## Comparison with FAT16

| Feature | Minix v2 | FAT16 |
|---------|----------|-------|
| Max filename | 30 chars | 8.3 (LFN: 255) |
| Permissions | Full Unix | None |
| Hard links | Yes | No |
| Timestamps | atime/mtime/ctime | mtime only |
| Max file size | 2GB+ | 2GB |
| Complexity | Medium | Simple |
| Linux tools | mkfs.minix | mkfs.fat |

## Limitations

- **Block size**: Fixed 1KB (Minix v2)
- **Max partition**: ~4GB practical limit
- **No journaling**: Risk of corruption on crash
- **Single-threaded**: VOS doesn't lock for concurrent access

## Summary

Minix filesystem provides:

1. **Unix semantics** - Proper inodes, permissions, links
2. **Persistent storage** - User data survives reboot
3. **Simple implementation** - Educational and maintainable
4. **Linux compatibility** - Easy to create and inspect images
5. **First-boot setup** - Interactive initialization

---

*Previous: [Chapter 17: RAM Filesystem](17_ramfs.md)*
*Next: [Chapter 19: Tasking and Scheduling](19_tasking.md)*
