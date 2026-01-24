# Chapter 16: Virtual File System

The Virtual File System (VFS) provides a unified interface for different filesystems. It allows the kernel and user programs to work with files without knowing which filesystem they're on.

## VFS Architecture

```
+---------------------+
|    User Programs    |
|  (open, read, write)|
+---------------------+
          |
+---------------------+
|        VFS          |
| (path resolution,   |
|  file descriptors)  |
+---------------------+
     |         |
+--------+ +--------+
| ramfs  | | FAT16  |
+--------+ +--------+
     |         |
  Memory    ATA Disk
```

## Core Data Structures

### VFS Node (vnode)

```c
typedef struct vfs_node {
    char name[256];
    uint32_t flags;         // VFS_FILE, VFS_DIRECTORY, etc.
    uint32_t length;        // File size
    uint32_t inode;         // Filesystem-specific identifier
    void *fs_data;          // Filesystem private data

    // Operations
    struct vfs_ops *ops;

    // Tree structure
    struct vfs_node *parent;
    struct vfs_node *children;
    struct vfs_node *next;

    // Mount point
    struct vfs_node *mount;
} vfs_node_t;
```

### VFS Operations

```c
typedef struct vfs_ops {
    int32_t (*open)(vfs_node_t *node, uint32_t flags);
    int32_t (*close)(vfs_node_t *node);
    int32_t (*read)(vfs_node_t *node, uint32_t offset,
                    uint32_t size, void *buffer);
    int32_t (*write)(vfs_node_t *node, uint32_t offset,
                     uint32_t size, const void *buffer);
    int32_t (*readdir)(vfs_node_t *node, uint32_t index,
                       struct dirent *entry);
    vfs_node_t* (*finddir)(vfs_node_t *node, const char *name);
    int32_t (*create)(vfs_node_t *parent, const char *name,
                      uint32_t flags);
    int32_t (*unlink)(vfs_node_t *parent, const char *name);
    int32_t (*mkdir)(vfs_node_t *parent, const char *name);
    int32_t (*rmdir)(vfs_node_t *parent, const char *name);
    int32_t (*stat)(vfs_node_t *node, struct stat *st);
    int32_t (*truncate)(vfs_node_t *node, uint32_t length);
} vfs_ops_t;
```

### File Descriptor

```c
typedef struct {
    vfs_node_t *node;
    uint32_t offset;
    uint32_t flags;
    int refcount;
} file_desc_t;
```

## Path Resolution

```c
vfs_node_t* vfs_resolve_path(const char *path) {
    if (!path || !*path) return NULL;

    vfs_node_t *node;

    // Start from root or current directory
    if (path[0] == '/') {
        node = vfs_root;
        path++;
    } else {
        node = current_task->cwd;
    }

    char component[256];

    while (*path) {
        // Skip slashes
        while (*path == '/') path++;
        if (!*path) break;

        // Extract component
        int i = 0;
        while (*path && *path != '/' && i < 255) {
            component[i++] = *path++;
        }
        component[i] = '\0';

        // Handle special entries
        if (strcmp(component, ".") == 0) {
            continue;
        }
        if (strcmp(component, "..") == 0) {
            if (node->parent) {
                node = node->parent;
            }
            continue;
        }

        // Check for mount point
        if (node->mount) {
            node = node->mount;
        }

        // Find child
        if (!(node->flags & VFS_DIRECTORY)) {
            return NULL;  // Not a directory
        }

        vfs_node_t *child = node->ops->finddir(node, component);
        if (!child) {
            return NULL;  // Not found
        }

        node = child;
    }

    // Check final mount point
    if (node->mount) {
        node = node->mount;
    }

    return node;
}
```

## Mount Points

```c
typedef struct mount_point {
    char path[256];
    vfs_node_t *node;
    struct mount_point *next;
} mount_point_t;

static mount_point_t *mounts = NULL;

int vfs_mount(const char *path, vfs_node_t *fs_root) {
    // Resolve mount point path
    vfs_node_t *mount_node = vfs_resolve_path(path);
    if (!mount_node) {
        // Create mount point directory
        vfs_mkdir(path);
        mount_node = vfs_resolve_path(path);
    }

    // Link filesystems
    mount_node->mount = fs_root;
    fs_root->parent = mount_node->parent;

    // Track mount
    mount_point_t *mp = kmalloc(sizeof(mount_point_t));
    strcpy(mp->path, path);
    mp->node = fs_root;
    mp->next = mounts;
    mounts = mp;

    return 0;
}

int vfs_umount(const char *path) {
    // Find and remove mount point
    mount_point_t **pp = &mounts;
    while (*pp) {
        if (strcmp((*pp)->path, path) == 0) {
            mount_point_t *mp = *pp;
            *pp = mp->next;

            // Unlink filesystems
            vfs_node_t *mount_node = vfs_resolve_path(path);
            if (mount_node) {
                mount_node->mount = NULL;
            }

            kfree(mp);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -ENOENT;
}
```

## File Descriptor Table

```c
#define MAX_FDS 256
static file_desc_t fd_table[MAX_FDS];

int vfs_alloc_fd(vfs_node_t *node, uint32_t flags) {
    for (int i = 3; i < MAX_FDS; i++) {  // Skip stdin/stdout/stderr
        if (fd_table[i].node == NULL) {
            fd_table[i].node = node;
            fd_table[i].offset = 0;
            fd_table[i].flags = flags;
            fd_table[i].refcount = 1;
            return i;
        }
    }
    return -EMFILE;
}

void vfs_free_fd(int fd) {
    if (fd >= 0 && fd < MAX_FDS) {
        fd_table[fd].refcount--;
        if (fd_table[fd].refcount <= 0) {
            fd_table[fd].node = NULL;
        }
    }
}
```

## File Operations

### Open

```c
int vfs_open(const char *path, uint32_t flags) {
    vfs_node_t *node = vfs_resolve_path(path);

    // Handle O_CREAT
    if (!node && (flags & O_CREAT)) {
        char *parent_path = dirname(path);
        char *filename = basename(path);

        vfs_node_t *parent = vfs_resolve_path(parent_path);
        if (!parent || !(parent->flags & VFS_DIRECTORY)) {
            return -ENOENT;
        }

        int ret = parent->ops->create(parent, filename, VFS_FILE);
        if (ret < 0) return ret;

        node = vfs_resolve_path(path);
    }

    if (!node) return -ENOENT;

    // Check permissions based on flags
    // ...

    // Call filesystem open
    if (node->ops->open) {
        int ret = node->ops->open(node, flags);
        if (ret < 0) return ret;
    }

    // Handle O_TRUNC
    if ((flags & O_TRUNC) && (flags & O_WRONLY || flags & O_RDWR)) {
        if (node->ops->truncate) {
            node->ops->truncate(node, 0);
        }
    }

    return vfs_alloc_fd(node, flags);
}
```

### Read

```c
int32_t vfs_read(int fd, void *buffer, uint32_t size) {
    if (fd < 0 || fd >= MAX_FDS) return -EBADF;

    file_desc_t *desc = &fd_table[fd];
    if (!desc->node) return -EBADF;

    if (!(desc->flags & O_RDONLY) && !(desc->flags & O_RDWR)) {
        return -EBADF;
    }

    vfs_node_t *node = desc->node;
    if (!node->ops->read) return -ENOSYS;

    int32_t bytes = node->ops->read(node, desc->offset, size, buffer);
    if (bytes > 0) {
        desc->offset += bytes;
    }

    return bytes;
}
```

### Write

```c
int32_t vfs_write(int fd, const void *buffer, uint32_t size) {
    if (fd < 0 || fd >= MAX_FDS) return -EBADF;

    file_desc_t *desc = &fd_table[fd];
    if (!desc->node) return -EBADF;

    if (!(desc->flags & O_WRONLY) && !(desc->flags & O_RDWR)) {
        return -EBADF;
    }

    vfs_node_t *node = desc->node;
    if (!node->ops->write) return -ENOSYS;

    // Handle O_APPEND
    if (desc->flags & O_APPEND) {
        desc->offset = node->length;
    }

    int32_t bytes = node->ops->write(node, desc->offset, size, buffer);
    if (bytes > 0) {
        desc->offset += bytes;
    }

    return bytes;
}
```

### Seek

```c
int32_t vfs_lseek(int fd, int32_t offset, int whence) {
    if (fd < 0 || fd >= MAX_FDS) return -EBADF;

    file_desc_t *desc = &fd_table[fd];
    if (!desc->node) return -EBADF;

    uint32_t new_offset;

    switch (whence) {
    case SEEK_SET:
        new_offset = offset;
        break;
    case SEEK_CUR:
        new_offset = desc->offset + offset;
        break;
    case SEEK_END:
        new_offset = desc->node->length + offset;
        break;
    default:
        return -EINVAL;
    }

    if (new_offset < 0) return -EINVAL;

    desc->offset = new_offset;
    return new_offset;
}
```

## Directory Operations

```c
int vfs_readdir(int fd, struct dirent *entry) {
    if (fd < 0 || fd >= MAX_FDS) return -EBADF;

    file_desc_t *desc = &fd_table[fd];
    if (!desc->node) return -EBADF;
    if (!(desc->node->flags & VFS_DIRECTORY)) return -ENOTDIR;

    vfs_node_t *node = desc->node;
    if (!node->ops->readdir) return -ENOSYS;

    int ret = node->ops->readdir(node, desc->offset, entry);
    if (ret == 0) {
        desc->offset++;
    }

    return ret;
}

int vfs_mkdir(const char *path) {
    char *parent_path = dirname(path);
    char *dirname = basename(path);

    vfs_node_t *parent = vfs_resolve_path(parent_path);
    if (!parent) return -ENOENT;
    if (!(parent->flags & VFS_DIRECTORY)) return -ENOTDIR;
    if (!parent->ops->mkdir) return -ENOSYS;

    return parent->ops->mkdir(parent, dirname);
}
```

## Standard I/O Setup

```c
void vfs_init_stdio(void) {
    // stdin (fd 0)
    fd_table[0].node = &console_node;
    fd_table[0].flags = O_RDONLY;
    fd_table[0].refcount = 1;

    // stdout (fd 1)
    fd_table[1].node = &console_node;
    fd_table[1].flags = O_WRONLY;
    fd_table[1].refcount = 1;

    // stderr (fd 2)
    fd_table[2].node = &console_node;
    fd_table[2].flags = O_WRONLY;
    fd_table[2].refcount = 1;
}
```

## VOS Mount Structure

```
/                   (ramfs root)
├── bin/            (initramfs binaries)
├── tmp/            (ramfs temporary)
├── disk/           (FAT16 partition)
│   ├── home/
│   ├── usr/
│   └── etc/
├── home -> /disk/home   (symlink alias)
├── usr -> /disk/usr     (symlink alias)
└── etc -> /disk/etc     (symlink alias)
```

## Summary

The VFS provides:

1. **Unified file interface** regardless of filesystem
2. **Path resolution** with mount point support
3. **File descriptors** for open file tracking
4. **Directory tree** navigation
5. **Pluggable filesystem** backends

This abstraction allows VOS to support multiple filesystems transparently.

---

*Previous: [Chapter 15: Serial Port](15_serial.md)*
*Next: [Chapter 17: RAM Filesystem](17_ramfs.md)*
