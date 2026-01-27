#include "dma.h"
#include "io.h"
#include "pmm.h"
#include "kheap.h"
#include "string.h"
#include "serial.h"

// Static DMA buffer in kernel BSS - guaranteed to be below 16MB since kernel loads at 1MB
// Aligned to 64KB boundary to avoid crossing 64KB boundary issues
// We use a linker section to ensure it's placed in low memory
#define STATIC_DMA_BUFFER_SIZE 65536
static uint8_t static_dma_buffer[STATIC_DMA_BUFFER_SIZE] __attribute__((aligned(65536)));
static bool static_dma_buffer_used = false;

// Page registers for each DMA channel
static const uint8_t dma_page_ports[8] = {
    0x87,  // Channel 0
    0x83,  // Channel 1 (SB16 8-bit)
    0x81,  // Channel 2
    0x82,  // Channel 3
    0x8F,  // Channel 4 (cascade, not used)
    0x8B,  // Channel 5 (SB16 16-bit)
    0x89,  // Channel 6
    0x8A   // Channel 7
};

// Address ports for each DMA channel
static const uint8_t dma_addr_ports[8] = {
    0x00,  // Channel 0
    0x02,  // Channel 1
    0x04,  // Channel 2
    0x06,  // Channel 3
    0xC0,  // Channel 4
    0xC4,  // Channel 5
    0xC8,  // Channel 6
    0xCC   // Channel 7
};

// Count ports for each DMA channel
static const uint8_t dma_count_ports[8] = {
    0x01,  // Channel 0
    0x03,  // Channel 1
    0x05,  // Channel 2
    0x07,  // Channel 3
    0xC2,  // Channel 4
    0xC6,  // Channel 5
    0xCA,  // Channel 6
    0xCE   // Channel 7
};

// Simple pool of DMA-safe physical pages (in low 16MB)
#define DMA_MAX_BUFFERS 8
static dma_buffer_t dma_buffers[DMA_MAX_BUFFERS];
static bool dma_buffer_used[DMA_MAX_BUFFERS];

void dma_init(void) {
    // Initialize buffer tracking
    for (int i = 0; i < DMA_MAX_BUFFERS; i++) {
        dma_buffer_used[i] = false;
        dma_buffers[i].virtual_addr = NULL;
        dma_buffers[i].physical_addr = 0;
        dma_buffers[i].size = 0;
    }

    // Reset DMA controllers
    outb(DMA1_MASTER_CLEAR, 0xFF);
    outb(DMA2_MASTER_CLEAR, 0xFF);

    // Enable DMA controller cascading (channel 4 cascades DMA1 to DMA2)
    outb(DMA2_MODE, DMA_MODE_CASCADE | 0x00);  // Channel 4 in cascade mode
    outb(DMA2_SINGLE_MASK, 0x00);  // Unmask channel 4

    serial_write_string("[DMA] Initialized\n");
}

dma_buffer_t* dma_alloc_buffer(uint32_t size) {
    if (size == 0 || size > STATIC_DMA_BUFFER_SIZE) {
        serial_write_string("[DMA] Invalid buffer size (max 64KB)\n");
        return NULL;
    }

    // Use the static DMA buffer (only one buffer supported for now)
    if (static_dma_buffer_used) {
        serial_write_string("[DMA] Static buffer already in use\n");
        return NULL;
    }

    // Find a free slot
    int slot = -1;
    for (int i = 0; i < DMA_MAX_BUFFERS; i++) {
        if (!dma_buffer_used[i]) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        serial_write_string("[DMA] No free buffer slots\n");
        return NULL;
    }

    // Get physical address of static buffer
    // In VOS, kernel virtual addresses below ~1GB equal physical addresses
    uint32_t phys_addr = (uint32_t)static_dma_buffer;

    // Verify it's below 16MB (it should be since kernel loads at 1MB)
    if (phys_addr >= 0x1000000) {
        serial_write_string("[DMA] Static buffer not in low memory! phys=0x");
        serial_write_hex(phys_addr);
        serial_write_string("\n");
        return NULL;
    }

    // Verify it doesn't cross 64KB boundary
    uint32_t end_phys = phys_addr + size - 1;
    if ((phys_addr & 0xFFFF0000) != (end_phys & 0xFFFF0000)) {
        serial_write_string("[DMA] Static buffer crosses 64KB boundary\n");
        return NULL;
    }

    dma_buffers[slot].virtual_addr = static_dma_buffer;
    dma_buffers[slot].physical_addr = phys_addr;
    dma_buffers[slot].size = size;
    dma_buffer_used[slot] = true;
    static_dma_buffer_used = true;

    // Zero the buffer
    memset(static_dma_buffer, 0, size);

    serial_write_string("[DMA] Allocated static buffer at phys=0x");
    serial_write_hex(phys_addr);
    serial_write_string(" size=0x");
    serial_write_hex(size);
    serial_write_string("\n");

    return &dma_buffers[slot];
}

void dma_free_buffer(dma_buffer_t* buffer) {
    if (!buffer) return;

    for (int i = 0; i < DMA_MAX_BUFFERS; i++) {
        if (dma_buffer_used[i] && &dma_buffers[i] == buffer) {
            // Check if this is the static buffer
            if (buffer->virtual_addr == static_dma_buffer) {
                static_dma_buffer_used = false;
            }

            buffer->virtual_addr = NULL;
            buffer->physical_addr = 0;
            buffer->size = 0;
            dma_buffer_used[i] = false;

            serial_write_string("[DMA] Freed buffer slot ");
            serial_write_dec(i);
            serial_write_string("\n");
            return;
        }
    }
}

void dma_setup_transfer(uint8_t channel, uint32_t phys_addr, uint16_t count, uint8_t mode) {
    if (channel > 7 || channel == 4) {
        return;  // Invalid channel
    }

    bool is_16bit = (channel >= 4);

    // Disable the channel first
    dma_stop(channel);

    // For 16-bit DMA (channels 5-7), addresses and counts are in 16-bit words
    uint32_t addr = phys_addr;
    uint16_t cnt = count;

    if (is_16bit) {
        // 16-bit DMA uses word addresses (divide by 2)
        addr = phys_addr >> 1;
        cnt = count >> 1;
    }

    // Clear the flip-flop
    if (is_16bit) {
        outb(DMA2_CLEAR_FF, 0xFF);
    } else {
        outb(DMA1_CLEAR_FF, 0xFF);
    }

    // Set the mode
    uint8_t mode_val = (channel & 0x03) | mode;
    if (is_16bit) {
        outb(DMA2_MODE, mode_val);
    } else {
        outb(DMA1_MODE, mode_val);
    }

    // Set the address (low byte, then high byte)
    outb(dma_addr_ports[channel], (uint8_t)(addr & 0xFF));
    outb(dma_addr_ports[channel], (uint8_t)((addr >> 8) & 0xFF));

    // Set the page register (bits 16-23 of physical address)
    outb(dma_page_ports[channel], (uint8_t)((phys_addr >> 16) & 0xFF));

    // Set the count (low byte, then high byte)
    outb(dma_count_ports[channel], (uint8_t)(cnt & 0xFF));
    outb(dma_count_ports[channel], (uint8_t)((cnt >> 8) & 0xFF));

    serial_write_string("[DMA] Setup ch");
    serial_write_hex(channel);
    serial_write_string(" addr=0x");
    serial_write_hex(phys_addr);
    serial_write_string(" count=0x");
    serial_write_hex(count);
    serial_write_string("\n");
}

void dma_start(uint8_t channel) {
    if (channel > 7 || channel == 4) {
        return;
    }

    // Unmask the channel (bit 2 = 0 means unmask)
    uint8_t mask_val = channel & 0x03;

    if (channel >= 4) {
        outb(DMA2_SINGLE_MASK, mask_val);
    } else {
        outb(DMA1_SINGLE_MASK, mask_val);
    }
}

void dma_stop(uint8_t channel) {
    if (channel > 7 || channel == 4) {
        return;
    }

    // Mask the channel (bit 2 = 1 means mask)
    uint8_t mask_val = (channel & 0x03) | 0x04;

    if (channel >= 4) {
        outb(DMA2_SINGLE_MASK, mask_val);
    } else {
        outb(DMA1_SINGLE_MASK, mask_val);
    }
}
