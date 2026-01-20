#ifndef ATA_H
#define ATA_H

#include "types.h"

// ATA PIO (primary channel, master) for QEMU/legacy IDE.
bool ata_init(void);
bool ata_is_present(void);
uint32_t ata_total_sectors(void);
const char* ata_model(void);

bool ata_read_sector(uint32_t lba, uint8_t* out512);
bool ata_write_sector(uint32_t lba, const uint8_t* in512);
bool ata_flush(void);

#endif

