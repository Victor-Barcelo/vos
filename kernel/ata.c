#include "ata.h"
#include "io.h"
#include "string.h"

enum {
    ATA_PRIMARY_IO = 0x1F0,
    ATA_PRIMARY_CTRL = 0x3F6,
};

enum {
    ATA_REG_DATA = 0,
    ATA_REG_ERROR = 1,
    ATA_REG_FEATURES = 1,
    ATA_REG_SECCOUNT0 = 2,
    ATA_REG_LBA0 = 3,
    ATA_REG_LBA1 = 4,
    ATA_REG_LBA2 = 5,
    ATA_REG_HDDEVSEL = 6,
    ATA_REG_COMMAND = 7,
    ATA_REG_STATUS = 7,
};

enum {
    ATA_SR_BSY = 0x80,
    ATA_SR_DRDY = 0x40,
    ATA_SR_DF = 0x20,
    ATA_SR_DRQ = 0x08,
    ATA_SR_ERR = 0x01,
};

enum {
    ATA_CMD_IDENTIFY = 0xEC,
    ATA_CMD_READ_SECTORS = 0x20,
    ATA_CMD_WRITE_SECTORS = 0x30,
    ATA_CMD_CACHE_FLUSH = 0xE7,
};

static bool g_present = false;
static uint32_t g_total_sectors = 0;
static char g_model[41];

static inline void cpu_pause(void) {
    __asm__ volatile ("pause");
}

static inline uint8_t ata_inb(uint16_t reg) {
    return inb((uint16_t)(ATA_PRIMARY_IO + reg));
}

static inline void ata_outb(uint16_t reg, uint8_t v) {
    outb((uint16_t)(ATA_PRIMARY_IO + reg), v);
}

static inline uint8_t ata_alt_status(void) {
    return inb(ATA_PRIMARY_CTRL);
}

static void ata_delay_400ns(void) {
    (void)ata_alt_status();
    (void)ata_alt_status();
    (void)ata_alt_status();
    (void)ata_alt_status();
}

static bool ata_wait_not_busy(uint32_t timeout) {
    for (uint32_t i = 0; i < timeout; i++) {
        uint8_t st = ata_alt_status();
        if ((st & ATA_SR_BSY) == 0) {
            return true;
        }
        if ((i & 0xFFu) == 0) {
            ata_delay_400ns();
        }
        cpu_pause();
    }
    return false;
}

static bool ata_wait_drq(uint32_t timeout) {
    for (uint32_t i = 0; i < timeout; i++) {
        uint8_t st = ata_alt_status();
        if (st & ATA_SR_ERR) {
            return false;
        }
        if (st & ATA_SR_DF) {
            return false;
        }
        if ((st & ATA_SR_BSY) == 0 && (st & ATA_SR_DRQ) != 0) {
            return true;
        }
        if ((i & 0xFFu) == 0) {
            ata_delay_400ns();
        }
        cpu_pause();
    }
    return false;
}

static void ata_select_drive(uint8_t drive_head) {
    ata_outb(ATA_REG_HDDEVSEL, drive_head);
    ata_delay_400ns();
}

static void ata_parse_model(char* out, uint32_t out_len, const uint16_t* id) {
    if (!out || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (!id) {
        return;
    }

    // Words 27..46: 40 ASCII chars, byte-swapped within each word.
    uint32_t pos = 0;
    for (uint32_t w = 27; w <= 46 && pos + 1 < out_len; w++) {
        uint16_t v = id[w];
        char a = (char)((v >> 8) & 0xFF);
        char b = (char)(v & 0xFF);
        if (pos + 1 < out_len) out[pos++] = a;
        if (pos + 1 < out_len) out[pos++] = b;
    }
    out[pos < out_len ? pos : (out_len - 1u)] = '\0';

    // Trim trailing spaces.
    int end = (int)strlen(out) - 1;
    while (end >= 0 && (out[end] == ' ' || out[end] == '\t')) {
        out[end--] = '\0';
    }
}

bool ata_init(void) {
    g_present = false;
    g_total_sectors = 0;
    memset(g_model, 0, sizeof(g_model));

    // Disable ATA interrupts (we use polling only).
    outb(ATA_PRIMARY_CTRL, 0x02);
    ata_delay_400ns();

    uint32_t irq_flags = irq_save();

    // Select primary master (CHS mode for IDENTIFY is fine).
    ata_select_drive(0xA0);

    ata_outb(ATA_REG_SECCOUNT0, 0);
    ata_outb(ATA_REG_LBA0, 0);
    ata_outb(ATA_REG_LBA1, 0);
    ata_outb(ATA_REG_LBA2, 0);

    ata_outb(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay_400ns();

    uint8_t st = ata_alt_status();
    if (st == 0) {
        irq_restore(irq_flags);
        return false; // no device
    }

    if (!ata_wait_not_busy(100000u)) {
        irq_restore(irq_flags);
        return false;
    }

    // Some ATAPI devices set LBA1/LBA2, but QEMU HDD won't.
    uint8_t lba1 = ata_inb(ATA_REG_LBA1);
    uint8_t lba2 = ata_inb(ATA_REG_LBA2);
    if (lba1 != 0 || lba2 != 0) {
        irq_restore(irq_flags);
        return false;
    }

    if (!ata_wait_drq(100000u)) {
        irq_restore(irq_flags);
        return false;
    }

    uint16_t id[256];
    for (uint32_t i = 0; i < 256; i++) {
        id[i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
    }

    // Total 28-bit LBA sectors: words 60-61.
    g_total_sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    ata_parse_model(g_model, sizeof(g_model), id);
    g_present = (g_total_sectors != 0);

    irq_restore(irq_flags);
    return g_present;
}

bool ata_is_present(void) {
    return g_present;
}

uint32_t ata_total_sectors(void) {
    return g_total_sectors;
}

const char* ata_model(void) {
    return g_model;
}

bool ata_read_sector(uint32_t lba, uint8_t* out512) {
    if (!g_present || !out512) {
        return false;
    }
    if (lba & 0xF0000000u) {
        return false; // 28-bit only
    }

    uint32_t irq_flags = irq_save();

    if (!ata_wait_not_busy(100000u)) {
        irq_restore(irq_flags);
        return false;
    }

    ata_select_drive((uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
    ata_outb(ATA_REG_SECCOUNT0, 1);
    ata_outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFFu));
    ata_outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFFu));
    ata_outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFFu));
    ata_outb(ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
    ata_delay_400ns();

    if (!ata_wait_drq(100000u)) {
        irq_restore(irq_flags);
        return false;
    }

    uint16_t* dst = (uint16_t*)out512;
    for (uint32_t i = 0; i < 256; i++) {
        dst[i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
    }

    if (!ata_wait_not_busy(100000u) || ((ata_alt_status() & (ATA_SR_ERR | ATA_SR_DF)) != 0)) {
        irq_restore(irq_flags);
        return false;
    }

    irq_restore(irq_flags);
    return true;
}

bool ata_flush(void) {
    if (!g_present) {
        return false;
    }

    uint32_t irq_flags = irq_save();

    if (!ata_wait_not_busy(100000u)) {
        irq_restore(irq_flags);
        return false;
    }
    ata_outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_delay_400ns();
    bool ok = ata_wait_not_busy(200000u);
    irq_restore(irq_flags);
    return ok;
}

bool ata_write_sector(uint32_t lba, const uint8_t* in512) {
    if (!g_present || !in512) {
        return false;
    }
    if (lba & 0xF0000000u) {
        return false; // 28-bit only
    }

    uint32_t irq_flags = irq_save();

    if (!ata_wait_not_busy(100000u)) {
        irq_restore(irq_flags);
        return false;
    }

    ata_select_drive((uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
    ata_outb(ATA_REG_SECCOUNT0, 1);
    ata_outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFFu));
    ata_outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFFu));
    ata_outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFFu));
    ata_outb(ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);
    ata_delay_400ns();

    if (!ata_wait_drq(100000u)) {
        irq_restore(irq_flags);
        return false;
    }

    const uint16_t* src = (const uint16_t*)in512;
    for (uint32_t i = 0; i < 256; i++) {
        outw(ATA_PRIMARY_IO + ATA_REG_DATA, src[i]);
    }

    bool ok = ata_wait_not_busy(200000u) && ((ata_inb(ATA_REG_STATUS) & (ATA_SR_ERR | ATA_SR_DF)) == 0);
    irq_restore(irq_flags);
    return ok;
}
