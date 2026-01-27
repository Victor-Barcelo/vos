#ifndef DMA_H
#define DMA_H

#include "types.h"

// ISA DMA controller ports
#define DMA1_STATUS_CMD     0x08  // Status (R) / Command (W) register
#define DMA1_REQUEST        0x09  // Request register
#define DMA1_SINGLE_MASK    0x0A  // Single channel mask register
#define DMA1_MODE           0x0B  // Mode register
#define DMA1_CLEAR_FF       0x0C  // Clear flip-flop register
#define DMA1_MASTER_CLEAR   0x0D  // Master clear register
#define DMA1_MASK_RESET     0x0E  // Clear mask register
#define DMA1_ALL_MASK       0x0F  // Multi-channel mask register

#define DMA2_STATUS_CMD     0xD0  // Status (R) / Command (W) register
#define DMA2_REQUEST        0xD2  // Request register
#define DMA2_SINGLE_MASK    0xD4  // Single channel mask register
#define DMA2_MODE           0xD6  // Mode register
#define DMA2_CLEAR_FF       0xD8  // Clear flip-flop register
#define DMA2_MASTER_CLEAR   0xDA  // Master clear register
#define DMA2_MASK_RESET     0xDC  // Clear mask register
#define DMA2_ALL_MASK       0xDE  // Multi-channel mask register

// DMA channel address/count ports (DMA1: 0-3, DMA2: 4-7)
// Channel 0: addr=0x00, count=0x01, page=0x87
// Channel 1: addr=0x02, count=0x03, page=0x83 (SB16 8-bit)
// Channel 2: addr=0x04, count=0x05, page=0x81
// Channel 3: addr=0x06, count=0x07, page=0x82
// Channel 4: cascade (used to cascade DMA1 to DMA2)
// Channel 5: addr=0xC4, count=0xC6, page=0x8B (SB16 16-bit)
// Channel 6: addr=0xC8, count=0xCA, page=0x89
// Channel 7: addr=0xCC, count=0xCE, page=0x8A

// DMA mode register bits
#define DMA_MODE_DEMAND      0x00
#define DMA_MODE_SINGLE      0x40
#define DMA_MODE_BLOCK       0x80
#define DMA_MODE_CASCADE     0xC0

#define DMA_MODE_INCR        0x00  // Address increment
#define DMA_MODE_DECR        0x20  // Address decrement

#define DMA_MODE_AUTO        0x10  // Auto-initialize

#define DMA_MODE_VERIFY      0x00
#define DMA_MODE_WRITE       0x04  // Memory to I/O
#define DMA_MODE_READ        0x08  // I/O to memory

// DMA buffer structure
typedef struct dma_buffer {
    void* virtual_addr;      // Virtual address for CPU access
    uint32_t physical_addr;  // Physical address for DMA controller
    uint32_t size;           // Buffer size in bytes
} dma_buffer_t;

// Initialize DMA subsystem
void dma_init(void);

// Allocate a DMA-safe buffer (< 16MB, doesn't cross 64KB boundary)
// Returns NULL on failure
dma_buffer_t* dma_alloc_buffer(uint32_t size);

// Free a previously allocated DMA buffer
void dma_free_buffer(dma_buffer_t* buffer);

// Set up a DMA transfer on the specified channel
// channel: 0-7 (1 for SB16 8-bit, 5 for SB16 16-bit)
// phys_addr: physical address of buffer
// count: number of bytes to transfer minus 1
// mode: DMA mode register value (combine DMA_MODE_* flags)
void dma_setup_transfer(uint8_t channel, uint32_t phys_addr, uint16_t count, uint8_t mode);

// Start DMA transfer (unmask channel)
void dma_start(uint8_t channel);

// Stop DMA transfer (mask channel)
void dma_stop(uint8_t channel);

#endif // DMA_H
