// MBR partition table parser
#include "mbr.h"
#include "ata.h"
#include "screen.h"
#include "string.h"

static mbr_partition_t g_partitions[4];
static bool g_mbr_valid = false;

bool mbr_read(void) {
    g_mbr_valid = false;
    memset(g_partitions, 0, sizeof(g_partitions));

    if (!ata_is_present()) {
        return false;
    }

    // Read MBR (sector 0)
    uint8_t sector[512];
    if (!ata_read_sector(0, sector)) {
        return false;
    }

    // Check MBR signature
    uint16_t sig = (uint16_t)sector[510] | ((uint16_t)sector[511] << 8);
    if (sig != MBR_SIGNATURE) {
        return false;
    }

    // Parse partition table (starts at offset 446)
    const mbr_partition_entry_t* entries = (const mbr_partition_entry_t*)&sector[446];

    for (int i = 0; i < 4; i++) {
        const mbr_partition_entry_t* e = &entries[i];
        mbr_partition_t* p = &g_partitions[i];

        if (e->type == MBR_TYPE_EMPTY || e->sector_count == 0) {
            p->valid = false;
            continue;
        }

        p->valid = true;
        p->bootable = (e->boot_flag == 0x80);
        p->type = e->type;
        p->lba_start = e->lba_start;
        p->sector_count = e->sector_count;
        p->size_mb = (uint32_t)((uint64_t)e->sector_count * 512 / (1024 * 1024));
    }

    g_mbr_valid = true;
    return true;
}

const mbr_partition_t* mbr_get_partition(int index) {
    if (index < 0 || index >= 4) {
        return NULL;
    }
    if (!g_partitions[index].valid) {
        return NULL;
    }
    return &g_partitions[index];
}

const char* mbr_type_name(uint8_t type) {
    switch (type) {
        case MBR_TYPE_EMPTY:      return "Empty";
        case MBR_TYPE_FAT12:      return "FAT12";
        case MBR_TYPE_FAT16_SM:   return "FAT16 (<32MB)";
        case MBR_TYPE_EXTENDED:   return "Extended";
        case MBR_TYPE_FAT16:      return "FAT16";
        case MBR_TYPE_NTFS:       return "NTFS/HPFS";
        case MBR_TYPE_FAT32:      return "FAT32";
        case MBR_TYPE_FAT32_LBA:  return "FAT32 (LBA)";
        case MBR_TYPE_FAT16_LBA:  return "FAT16 (LBA)";
        case MBR_TYPE_EXTENDED_LBA: return "Extended (LBA)";
        case MBR_TYPE_LINUX_SWAP: return "Linux swap";
        case MBR_TYPE_LINUX:      return "Linux";
        case MBR_TYPE_MINIX_OLD:  return "Minix (old)";
        case MBR_TYPE_MINIX:      return "Minix";
        default:                  return "Unknown";
    }
}

void mbr_print_table(void) {
    if (!g_mbr_valid) {
        screen_println("[MBR] No valid MBR");
        return;
    }

    screen_println("[MBR] Partition table:");
    for (int i = 0; i < 4; i++) {
        const mbr_partition_t* p = &g_partitions[i];
        if (!p->valid) {
            continue;
        }
        screen_print("  ");
        screen_print_dec(i + 1);
        screen_print(": ");
        screen_print(p->bootable ? "*" : " ");
        screen_print(" ");
        screen_print(mbr_type_name(p->type));
        screen_print(" - ");
        screen_print_dec((int32_t)p->size_mb);
        screen_println(" MB");
    }
}

int mbr_find_partition_by_type(uint8_t type) {
    for (int i = 0; i < 4; i++) {
        if (g_partitions[i].valid && g_partitions[i].type == type) {
            return i;
        }
    }
    return -1;
}
