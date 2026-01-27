// MBR (Master Boot Record) partition table parser
#ifndef MBR_H
#define MBR_H

#include "types.h"

// MBR signature
#define MBR_SIGNATURE       0xAA55

// Common partition types
#define MBR_TYPE_EMPTY      0x00
#define MBR_TYPE_FAT12      0x01
#define MBR_TYPE_FAT16_SM   0x04    // FAT16 < 32MB
#define MBR_TYPE_EXTENDED   0x05
#define MBR_TYPE_FAT16      0x06    // FAT16 >= 32MB
#define MBR_TYPE_NTFS       0x07
#define MBR_TYPE_FAT32      0x0B
#define MBR_TYPE_FAT32_LBA  0x0C
#define MBR_TYPE_FAT16_LBA  0x0E
#define MBR_TYPE_EXTENDED_LBA 0x0F
#define MBR_TYPE_LINUX_SWAP 0x82
#define MBR_TYPE_LINUX      0x83    // Linux native (ext2/3/4, etc.)
#define MBR_TYPE_MINIX_OLD  0x80    // Old Minix
#define MBR_TYPE_MINIX      0x81    // Minix

// MBR partition entry (16 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  boot_flag;         // 0x80 = bootable, 0x00 = not bootable
    uint8_t  chs_start[3];      // CHS start address (legacy)
    uint8_t  type;              // Partition type
    uint8_t  chs_end[3];        // CHS end address (legacy)
    uint32_t lba_start;         // LBA start sector
    uint32_t sector_count;      // Number of sectors
} mbr_partition_entry_t;

// MBR structure (512 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  bootstrap[446];    // Boot code
    mbr_partition_entry_t partitions[4];  // Partition table (4 entries)
    uint16_t signature;         // 0xAA55
} mbr_t;

// Parsed partition info (more convenient)
typedef struct {
    bool     valid;             // Partition exists and is valid
    bool     bootable;          // Boot flag set
    uint8_t  type;              // Partition type code
    uint32_t lba_start;         // Start LBA
    uint32_t sector_count;      // Number of sectors
    uint32_t size_mb;           // Size in megabytes (approximate)
} mbr_partition_t;

// Read and parse MBR from disk
// Returns true if valid MBR found
bool mbr_read(void);

// Get partition info (0-3)
// Returns NULL if partition doesn't exist
const mbr_partition_t* mbr_get_partition(int index);

// Get partition type name
const char* mbr_type_name(uint8_t type);

// Print partition table (for debugging)
void mbr_print_table(void);

// Find first partition of a given type
// Returns partition index (0-3) or -1 if not found
int mbr_find_partition_by_type(uint8_t type);

#endif // MBR_H
