# Chapter 14: ATA Disk Driver

## ATA/IDE Protocol

ATA (Advanced Technology Attachment) is the standard interface for IDE hard drives. VOS implements the basic PIO (Programmed I/O) mode.

### ATA Ports

Primary ATA controller:

| Port | Read | Write |
|------|------|-------|
| 0x1F0 | Data | Data |
| 0x1F1 | Error | Features |
| 0x1F2 | Sector Count | Sector Count |
| 0x1F3 | LBA Low | LBA Low |
| 0x1F4 | LBA Mid | LBA Mid |
| 0x1F5 | LBA High | LBA High |
| 0x1F6 | Drive/Head | Drive/Head |
| 0x1F7 | Status | Command |
| 0x3F6 | Alt Status | Device Control |

Secondary ATA controller uses 0x170-0x177 and 0x376.

### Status Register

```
Bit 7: BSY  - Drive busy
Bit 6: DRDY - Drive ready
Bit 5: DF   - Drive fault
Bit 4: SRV  - Overlapped mode service request
Bit 3: DRQ  - Data request ready
Bit 2: CORR - Corrected data
Bit 1: IDX  - Index mark
Bit 0: ERR  - Error
```

### Common Commands

| Command | Value | Description |
|---------|-------|-------------|
| IDENTIFY | 0xEC | Get drive info |
| READ_SECTORS | 0x20 | Read sectors (PIO) |
| WRITE_SECTORS | 0x30 | Write sectors (PIO) |
| CACHE_FLUSH | 0xE7 | Flush write cache |

## Drive Detection

```c
typedef struct {
    uint16_t base_port;
    uint16_t ctrl_port;
    uint8_t  slave;
    bool     present;
    uint32_t sectors;
    char     model[41];
} ata_drive_t;

static ata_drive_t drives[4];

void ata_init(void) {
    // Primary master/slave
    detect_drive(&drives[0], 0x1F0, 0x3F6, 0);
    detect_drive(&drives[1], 0x1F0, 0x3F6, 1);

    // Secondary master/slave
    detect_drive(&drives[2], 0x170, 0x376, 0);
    detect_drive(&drives[3], 0x170, 0x376, 1);
}

static void detect_drive(ata_drive_t *drive, uint16_t base,
                         uint16_t ctrl, uint8_t slave) {
    drive->base_port = base;
    drive->ctrl_port = ctrl;
    drive->slave = slave;
    drive->present = false;

    // Select drive
    outb(base + 6, 0xA0 | (slave << 4));
    ata_delay(base);

    // Send IDENTIFY command
    outb(base + 7, 0xEC);
    ata_delay(base);

    // Check if drive exists
    if (inb(base + 7) == 0) return;

    // Wait for BSY clear
    if (!ata_wait(base, 0, 0x80)) return;

    // Check for errors
    uint8_t status = inb(base + 7);
    if (status & 0x01) return;  // ERR

    // Wait for DRQ
    if (!ata_wait(base, 0x08, 0)) return;

    // Read identify data
    uint16_t identify[256];
    for (int i = 0; i < 256; i++) {
        identify[i] = inw(base);
    }

    // Extract info
    drive->present = true;
    drive->sectors = identify[60] | (identify[61] << 16);

    // Extract model string (swapped bytes)
    for (int i = 0; i < 20; i++) {
        drive->model[i*2] = identify[27+i] >> 8;
        drive->model[i*2+1] = identify[27+i] & 0xFF;
    }
    drive->model[40] = '\0';
    trim_string(drive->model);
}
```

## LBA Addressing

VOS uses LBA28 (28-bit Logical Block Addressing):

```c
static void ata_select_sector(ata_drive_t *drive, uint32_t lba) {
    uint16_t base = drive->base_port;

    // Select drive and high 4 bits of LBA
    outb(base + 6, 0xE0 | (drive->slave << 4) | ((lba >> 24) & 0x0F));
    ata_delay(base);

    // Sector count
    outb(base + 2, 1);

    // LBA low, mid, high
    outb(base + 3, lba & 0xFF);
    outb(base + 4, (lba >> 8) & 0xFF);
    outb(base + 5, (lba >> 16) & 0xFF);
}
```

### LBA28 Limits

- Maximum sectors: 2^28 = 268,435,456
- Sector size: 512 bytes
- Maximum disk size: 128 GB

For larger disks, LBA48 would be needed.

## Reading Sectors

```c
bool ata_read_sectors(ata_drive_t *drive, uint32_t lba,
                      uint8_t count, void *buffer) {
    uint16_t base = drive->base_port;
    uint16_t *buf = (uint16_t *)buffer;

    for (uint8_t i = 0; i < count; i++) {
        // Select sector
        ata_select_sector(drive, lba + i);

        // Send READ command
        outb(base + 7, 0x20);

        // Wait for drive ready
        if (!ata_wait(base, 0x08, 0x80)) {
            return false;
        }

        // Read 256 words (512 bytes)
        for (int j = 0; j < 256; j++) {
            buf[j] = inw(base);
        }
        buf += 256;
    }

    return true;
}
```

## Writing Sectors

```c
bool ata_write_sectors(ata_drive_t *drive, uint32_t lba,
                       uint8_t count, const void *buffer) {
    uint16_t base = drive->base_port;
    const uint16_t *buf = (const uint16_t *)buffer;

    for (uint8_t i = 0; i < count; i++) {
        // Select sector
        ata_select_sector(drive, lba + i);

        // Send WRITE command
        outb(base + 7, 0x30);

        // Wait for drive ready
        if (!ata_wait(base, 0x08, 0x80)) {
            return false;
        }

        // Write 256 words (512 bytes)
        for (int j = 0; j < 256; j++) {
            outw(base, buf[j]);
        }
        buf += 256;

        // Flush cache
        outb(base + 7, 0xE7);
        ata_wait(base, 0, 0x80);
    }

    return true;
}
```

## Waiting for Drive

```c
static void ata_delay(uint16_t base) {
    // 400ns delay by reading status 4 times
    inb(base + 7);
    inb(base + 7);
    inb(base + 7);
    inb(base + 7);
}

static bool ata_wait(uint16_t base, uint8_t set, uint8_t clear) {
    uint32_t timeout = 100000;

    while (timeout--) {
        uint8_t status = inb(base + 7);

        if (status & 0x01) {
            // Error
            return false;
        }

        if ((status & set) == set && (status & clear) == 0) {
            return true;
        }
    }

    return false;  // Timeout
}
```

## Block Device Interface

VOS provides a generic block device interface:

```c
typedef struct {
    bool (*read)(uint32_t lba, uint8_t count, void *buffer);
    bool (*write)(uint32_t lba, uint8_t count, const void *buffer);
    uint32_t sector_size;
    uint32_t sector_count;
} block_device_t;

static block_device_t ata_device = {
    .read = ata_read,
    .write = ata_write,
    .sector_size = 512,
    .sector_count = 0
};

bool ata_read(uint32_t lba, uint8_t count, void *buffer) {
    return ata_read_sectors(&drives[0], lba, count, buffer);
}

bool ata_write(uint32_t lba, uint8_t count, const void *buffer) {
    return ata_write_sectors(&drives[0], lba, count, buffer);
}
```

## Partition Table

MBR partition table at sector 0:

```c
typedef struct {
    uint8_t  boot_flag;
    uint8_t  chs_start[3];
    uint8_t  type;
    uint8_t  chs_end[3];
    uint32_t lba_start;
    uint32_t sector_count;
} __attribute__((packed)) partition_entry_t;

typedef struct {
    uint8_t bootstrap[446];
    partition_entry_t partitions[4];
    uint16_t signature;  // 0xAA55
} __attribute__((packed)) mbr_t;

void read_partition_table(void) {
    mbr_t mbr;
    ata_read(0, 1, &mbr);

    if (mbr.signature != 0xAA55) {
        serial_printf("Invalid MBR signature\n");
        return;
    }

    for (int i = 0; i < 4; i++) {
        partition_entry_t *p = &mbr.partitions[i];
        if (p->type != 0) {
            serial_printf("Partition %d: type=%02x start=%u size=%u\n",
                         i, p->type, p->lba_start, p->sector_count);
        }
    }
}
```

### Common Partition Types

| Type | Description |
|------|-------------|
| 0x00 | Empty |
| 0x04 | FAT16 (<32MB) |
| 0x06 | FAT16 (32MB-2GB) |
| 0x0B | FAT32 |
| 0x0C | FAT32 LBA |
| 0x0E | FAT16 LBA |
| 0x83 | Linux |

## Integration with VFS

```c
void disk_init(void) {
    ata_init();

    if (!drives[0].present) {
        serial_printf("No ATA drive found\n");
        return;
    }

    serial_printf("ATA: %s (%u MB)\n",
                 drives[0].model,
                 drives[0].sectors / 2048);

    // Read partition table
    read_partition_table();

    // Mount first FAT16 partition
    for (int i = 0; i < 4; i++) {
        mbr_t mbr;
        ata_read(0, 1, &mbr);
        partition_entry_t *p = &mbr.partitions[i];

        if (p->type == 0x04 || p->type == 0x06 || p->type == 0x0E) {
            fat16_mount(&ata_device, p->lba_start);
            vfs_mount("/disk", &fat16_fs);
            break;
        }
    }
}
```

## Summary

The VOS ATA driver provides:

1. **Drive detection** using IDENTIFY command
2. **LBA28 addressing** for up to 128 GB
3. **PIO mode read/write** for simplicity
4. **MBR partition parsing** for disk layout
5. **Block device interface** for filesystem use

This enables persistent storage through the FAT16 filesystem.

---

*Previous: [Chapter 13: Mouse Driver](13_mouse.md)*
*Next: [Chapter 15: Serial Port](15_serial.md)*
