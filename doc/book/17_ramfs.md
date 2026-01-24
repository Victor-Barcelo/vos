# Chapter 17: RAM Filesystem

The RAM filesystem (ramfs) provides in-memory file storage. VOS uses it for the initial root filesystem and temporary storage.

## Ramfs Structure

```c
typedef struct ramfs_node {
    char name[256];
    uint32_t flags;         // RAMFS_FILE or RAMFS_DIRECTORY
    uint8_t *data;          // File contents
    uint32_t size;          // Current size
    uint32_t capacity;      // Allocated size
    struct ramfs_node *parent;
    struct ramfs_node *children;
    struct ramfs_node *next;
} ramfs_node_t;
```

## Node Management

### Create Node

```c
static ramfs_node_t* ramfs_create_node(const char *name, uint32_t flags) {
    ramfs_node_t *node = kcalloc(1, sizeof(ramfs_node_t));

    strncpy(node->name, name, 255);
    node->flags = flags;
    node->data = NULL;
    node->size = 0;
    node->capacity = 0;

    return node;
}
```

### Add Child

```c
static void ramfs_add_child(ramfs_node_t *parent, ramfs_node_t *child) {
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}
```

### Find Child

```c
static ramfs_node_t* ramfs_find_child(ramfs_node_t *parent, const char *name) {
    ramfs_node_t *child = parent->children;

    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next;
    }

    return NULL;
}
```

## File Operations

### Read

```c
static int32_t ramfs_read(vfs_node_t *vnode, uint32_t offset,
                          uint32_t size, void *buffer) {
    ramfs_node_t *node = (ramfs_node_t *)vnode->fs_data;

    if (offset >= node->size) return 0;

    uint32_t to_read = size;
    if (offset + to_read > node->size) {
        to_read = node->size - offset;
    }

    memcpy(buffer, node->data + offset, to_read);
    return to_read;
}
```

### Write

```c
static int32_t ramfs_write(vfs_node_t *vnode, uint32_t offset,
                           uint32_t size, const void *buffer) {
    ramfs_node_t *node = (ramfs_node_t *)vnode->fs_data;

    uint32_t end = offset + size;

    // Grow buffer if needed
    if (end > node->capacity) {
        uint32_t new_capacity = end + 4096;  // Add some extra
        new_capacity = (new_capacity + 4095) & ~4095;  // Align to page

        uint8_t *new_data = krealloc(node->data, new_capacity);
        if (!new_data) return -ENOMEM;

        // Zero new space
        if (new_capacity > node->capacity) {
            memset(new_data + node->capacity, 0,
                   new_capacity - node->capacity);
        }

        node->data = new_data;
        node->capacity = new_capacity;
    }

    memcpy(node->data + offset, buffer, size);

    if (end > node->size) {
        node->size = end;
        vnode->length = end;
    }

    return size;
}
```

### Truncate

```c
static int32_t ramfs_truncate(vfs_node_t *vnode, uint32_t length) {
    ramfs_node_t *node = (ramfs_node_t *)vnode->fs_data;

    if (length == 0) {
        kfree(node->data);
        node->data = NULL;
        node->size = 0;
        node->capacity = 0;
    } else if (length < node->size) {
        node->size = length;
    } else if (length > node->size) {
        // Extend with zeros
        ramfs_write(vnode, node->size, length - node->size, NULL);
    }

    vnode->length = node->size;
    return 0;
}
```

## Directory Operations

### Create File

```c
static int32_t ramfs_create(vfs_node_t *parent, const char *name,
                            uint32_t flags) {
    ramfs_node_t *parent_node = (ramfs_node_t *)parent->fs_data;

    // Check if already exists
    if (ramfs_find_child(parent_node, name)) {
        return -EEXIST;
    }

    // Create new node
    ramfs_node_t *node = ramfs_create_node(name, RAMFS_FILE);
    ramfs_add_child(parent_node, node);

    // Create VFS node
    vfs_node_t *vnode = vfs_create_node(name, VFS_FILE);
    vnode->fs_data = node;
    vnode->ops = &ramfs_ops;
    vfs_add_child(parent, vnode);

    return 0;
}
```

### Create Directory

```c
static int32_t ramfs_mkdir(vfs_node_t *parent, const char *name) {
    ramfs_node_t *parent_node = (ramfs_node_t *)parent->fs_data;

    if (ramfs_find_child(parent_node, name)) {
        return -EEXIST;
    }

    ramfs_node_t *node = ramfs_create_node(name, RAMFS_DIRECTORY);
    ramfs_add_child(parent_node, node);

    vfs_node_t *vnode = vfs_create_node(name, VFS_DIRECTORY);
    vnode->fs_data = node;
    vnode->ops = &ramfs_ops;
    vfs_add_child(parent, vnode);

    return 0;
}
```

### Read Directory

```c
static int32_t ramfs_readdir(vfs_node_t *vnode, uint32_t index,
                             struct dirent *entry) {
    ramfs_node_t *node = (ramfs_node_t *)vnode->fs_data;
    ramfs_node_t *child = node->children;

    // Skip to index
    uint32_t i = 0;
    while (child && i < index) {
        child = child->next;
        i++;
    }

    if (!child) return -1;  // End of directory

    entry->d_ino = (uint32_t)child;
    entry->d_type = (child->flags & RAMFS_DIRECTORY) ? DT_DIR : DT_REG;
    strncpy(entry->d_name, child->name, 255);

    return 0;
}
```

### Delete File

```c
static int32_t ramfs_unlink(vfs_node_t *parent, const char *name) {
    ramfs_node_t *parent_node = (ramfs_node_t *)parent->fs_data;
    ramfs_node_t *node = ramfs_find_child(parent_node, name);

    if (!node) return -ENOENT;
    if (node->flags & RAMFS_DIRECTORY) return -EISDIR;

    // Remove from parent's children list
    ramfs_node_t **pp = &parent_node->children;
    while (*pp) {
        if (*pp == node) {
            *pp = node->next;
            break;
        }
        pp = &(*pp)->next;
    }

    // Free node
    kfree(node->data);
    kfree(node);

    // Also remove VFS node
    vfs_remove_child(parent, name);

    return 0;
}
```

## Initramfs Integration

VOS loads an initramfs from the Multiboot module (tar format).

### Tar Header

```c
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
} tar_header_t;
```

### Parse Tar

```c
static uint32_t parse_octal(const char *str, int len) {
    uint32_t result = 0;
    for (int i = 0; i < len && str[i] >= '0' && str[i] <= '7'; i++) {
        result = (result << 3) | (str[i] - '0');
    }
    return result;
}

void ramfs_load_tar(void *tar_data, uint32_t tar_size) {
    uint8_t *ptr = tar_data;
    uint8_t *end = ptr + tar_size;

    while (ptr + 512 <= end) {
        tar_header_t *header = (tar_header_t *)ptr;

        // Check for end of archive
        if (header->name[0] == '\0') break;

        uint32_t size = parse_octal(header->size, 11);
        char *data = (char *)(ptr + 512);

        // Build path
        char path[256] = "/";
        if (header->prefix[0]) {
            strcat(path, header->prefix);
            strcat(path, "/");
        }
        strcat(path, header->name);

        // Create entry
        switch (header->typeflag) {
        case '0':
        case '\0':  // Regular file
            ramfs_create_file(path, data, size);
            break;
        case '5':   // Directory
            ramfs_create_dir(path);
            break;
        }

        // Move to next header (512-byte aligned)
        uint32_t blocks = (size + 511) / 512;
        ptr += 512 + blocks * 512;
    }
}
```

### Create File from Tar

```c
static void ramfs_create_file(const char *path, void *data, uint32_t size) {
    // Create parent directories
    char parent[256];
    strcpy(parent, path);
    char *last_slash = strrchr(parent, '/');
    if (last_slash && last_slash != parent) {
        *last_slash = '\0';
        ramfs_create_dir(parent);
    }

    // Create file
    int fd = vfs_open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd >= 0) {
        vfs_write(fd, data, size);
        vfs_close(fd);
    }
}
```

## Memory Usage

Ramfs stores files entirely in RAM:

```c
void ramfs_stats(uint32_t *files, uint32_t *bytes) {
    *files = 0;
    *bytes = 0;

    count_nodes(ramfs_root, files, bytes);
}

static void count_nodes(ramfs_node_t *node, uint32_t *files, uint32_t *bytes) {
    if (node->flags & RAMFS_FILE) {
        (*files)++;
        *bytes += node->capacity;
    }

    ramfs_node_t *child = node->children;
    while (child) {
        count_nodes(child, files, bytes);
        child = child->next;
    }
}
```

## Operations Table

```c
static vfs_ops_t ramfs_ops = {
    .open = ramfs_open,
    .close = ramfs_close,
    .read = ramfs_read,
    .write = ramfs_write,
    .readdir = ramfs_readdir,
    .finddir = ramfs_finddir,
    .create = ramfs_create,
    .unlink = ramfs_unlink,
    .mkdir = ramfs_mkdir,
    .rmdir = ramfs_rmdir,
    .stat = ramfs_stat,
    .truncate = ramfs_truncate
};
```

## Summary

Ramfs provides:

1. **In-memory storage** for fast access
2. **Full read/write** capability
3. **Directory structure** support
4. **Tar loading** from initramfs module
5. **Simple implementation** for bootstrap

It serves as the root filesystem and /tmp storage in VOS.

---

*Previous: [Chapter 16: Virtual File System](16_vfs.md)*
*Next: [Chapter 18: FAT16 Filesystem](18_fat16.md)*
