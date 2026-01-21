#ifndef RAMFS_H
#define RAMFS_H

#include "types.h"

typedef struct ramfs_dirent {
    char name[64];
    bool is_dir;
    uint32_t size;
    // FAT-style "last write" timestamp for the entry (0 means unknown/unset).
    uint16_t wtime;
    uint16_t wdate;
} ramfs_dirent_t;

void ramfs_init(void);

bool ramfs_is_dir(const char* path);
bool ramfs_is_file(const char* path);

bool ramfs_stat_ex(const char* path, bool* out_is_dir, uint32_t* out_size, uint16_t* out_wtime, uint16_t* out_wdate);

bool ramfs_mkdir(const char* path);
bool ramfs_write_file(const char* path, const uint8_t* data, uint32_t size, bool overwrite);
bool ramfs_rename(const char* old_path, const char* new_path);

bool ramfs_read_file(const char* path, const uint8_t** out_data, uint32_t* out_size);
uint32_t ramfs_list_dir(const char* path, ramfs_dirent_t* out, uint32_t max);

#endif
