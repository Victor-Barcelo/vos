#include "sb16.h"
#include "dma.h"
#include "io.h"
#include "interrupts.h"
#include "timer.h"
#include "serial.h"
#include "string.h"

// Driver state
static bool sb16_present = false;
static uint16_t dsp_version = 0;
static sb16_format_t current_format = {44100, 16, 2};

// Double buffering for gapless audio playback
// We use a single DMA buffer split into two halves (ping-pong)
#define AUDIO_BUFFER_SIZE 32768  // Total buffer: 32KB (16KB per half)
#define AUDIO_HALF_SIZE   (AUDIO_BUFFER_SIZE / 2)

static dma_buffer_t* audio_buffer = NULL;
static volatile int current_half = 0;      // Which half is being played (0 or 1)
static volatile bool half_ready[2] = {true, true};  // Which halves need data
static volatile bool playing = false;
static volatile bool auto_init_active = false;
static volatile uint32_t irq_count = 0;

// Wait for DSP to be ready for writing
static bool dsp_write_ready(void) {
    for (int i = 0; i < 10000; i++) {
        if ((inb(SB_DSP_STATUS) & 0x80) == 0) {
            return true;
        }
    }
    return false;
}

// Write a byte to the DSP
static bool dsp_write(uint8_t value) {
    if (!dsp_write_ready()) {
        return false;
    }
    outb(SB_DSP_WRITE, value);
    return true;
}

// Wait for DSP to have data available
static bool dsp_read_ready(void) {
    for (int i = 0; i < 10000; i++) {
        if (inb(SB_BASE + 0x0E) & 0x80) {  // Read status at 0x22E
            return true;
        }
    }
    return false;
}

// Read a byte from the DSP
static uint8_t dsp_read(void) {
    if (!dsp_read_ready()) {
        return 0xFF;
    }
    return inb(SB_DSP_READ);
}

// Reset the DSP
static bool dsp_reset(void) {
    // Write 1 to reset port
    outb(SB_DSP_RESET, 1);

    // Wait at least 3 microseconds
    for (int i = 0; i < 100; i++) {
        inb(SB_DSP_RESET);  // Small delay
    }

    // Write 0 to reset port
    outb(SB_DSP_RESET, 0);

    // Wait for ready signal (0xAA)
    for (int i = 0; i < 100; i++) {
        if (dsp_read_ready()) {
            uint8_t val = inb(SB_DSP_READ);
            if (val == 0xAA) {
                return true;
            }
        }
        for (int j = 0; j < 100; j++) {
            inb(SB_DSP_RESET);  // Small delay
        }
    }

    return false;
}

// IRQ handler for SB16 - called when a buffer half finishes playing
static void sb16_irq_handler(interrupt_frame_t* frame) {
    (void)frame;

    irq_count++;

    // Mark the half that just finished as needing new data
    half_ready[current_half] = true;

    // Switch to the other half
    current_half = 1 - current_half;

    // Acknowledge the interrupt
    if (current_format.bits == 16) {
        inb(SB_DSP_INTR_ACK_16);
    } else {
        inb(SB_DSP_INTR_ACK);
    }
}

int sb16_init(void) {
    serial_write_string("[SB16] Initializing Sound Blaster 16...\n");

    // Try to reset the DSP
    if (!dsp_reset()) {
        serial_write_string("[SB16] DSP reset failed - card not present?\n");
        sb16_present = false;
        return -1;
    }

    // Get DSP version
    dsp_write(DSP_CMD_GET_VERSION);
    uint8_t major = dsp_read();
    uint8_t minor = dsp_read();
    dsp_version = ((uint16_t)major << 8) | minor;

    serial_write_string("[SB16] DSP version: ");
    serial_write_dec(major);
    serial_write_string(".");
    serial_write_dec(minor);
    serial_write_string("\n");

    // SB16 requires version 4.xx or higher
    if (major < 4) {
        serial_write_string("[SB16] Not a Sound Blaster 16 (need DSP 4.xx+)\n");
        sb16_present = false;
        return -1;
    }

    // Allocate DMA buffer (will be split into two halves)
    audio_buffer = dma_alloc_buffer(AUDIO_BUFFER_SIZE);
    if (!audio_buffer) {
        serial_write_string("[SB16] Failed to allocate DMA buffer\n");
        sb16_present = false;
        return -1;
    }

    // Clear the buffer
    memset(audio_buffer->virtual_addr, 0, AUDIO_BUFFER_SIZE);

    // Register IRQ handler
    irq_register_handler(SB_DEFAULT_IRQ, sb16_irq_handler);

    // Unmask IRQ5
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~(1 << SB_DEFAULT_IRQ));

    // Configure mixer for IRQ and DMA
    outb(SB_MIXER_ADDR, MIXER_INT_SETUP);
    outb(SB_MIXER_DATA, 0x02);  // IRQ 5

    outb(SB_MIXER_ADDR, MIXER_DMA_SETUP);
    outb(SB_MIXER_DATA, 0x22);  // DMA 1 (8-bit) and DMA 5 (16-bit)

    // Set master volume to max
    sb16_set_volume(255);

    // Turn speaker on
    dsp_write(DSP_CMD_SPEAKER_ON);

    sb16_present = true;
    serial_write_string("[SB16] Initialization complete (double-buffered)\n");

    return 0;
}

bool sb16_detected(void) {
    return sb16_present;
}

uint16_t sb16_get_version(void) {
    return dsp_version;
}

int sb16_set_format(uint32_t sample_rate, uint8_t bits, uint8_t channels) {
    if (!sb16_present) {
        return -1;
    }

    if (bits != 8 && bits != 16) {
        return -1;
    }

    if (channels != 1 && channels != 2) {
        return -1;
    }

    if (sample_rate < 5000 || sample_rate > 44100) {
        return -1;
    }

    // Stop any current playback before changing format
    if (auto_init_active) {
        sb16_stop();
    }

    current_format.sample_rate = sample_rate;
    current_format.bits = bits;
    current_format.channels = channels;

    // Set sample rate using SB16 command
    dsp_write(DSP_CMD_SET_SAMPLE_RATE);
    dsp_write((uint8_t)(sample_rate >> 8));    // High byte
    dsp_write((uint8_t)(sample_rate & 0xFF));  // Low byte

    serial_write_string("[SB16] Format set: ");
    serial_write_dec(sample_rate);
    serial_write_string("Hz, ");
    serial_write_dec(bits);
    serial_write_string("-bit, ");
    serial_write_string(channels == 2 ? "stereo" : "mono");
    serial_write_string("\n");

    return 0;
}

// Start auto-init DMA playback
static void start_auto_init_playback(void) {
    if (auto_init_active) {
        return;
    }

    uint8_t dma_channel;
    uint8_t dma_mode;

    // Set up DMA for auto-init mode on full buffer
    if (current_format.bits == 16) {
        dma_channel = SB_DEFAULT_DMA_16;
        dma_mode = DMA_MODE_SINGLE | DMA_MODE_READ | DMA_MODE_AUTO;
    } else {
        dma_channel = SB_DEFAULT_DMA_8;
        dma_mode = DMA_MODE_SINGLE | DMA_MODE_READ | DMA_MODE_AUTO;
    }

    // Program DMA for the full buffer (auto-init will loop it)
    uint16_t count = (uint16_t)(AUDIO_BUFFER_SIZE - 1);
    dma_setup_transfer(dma_channel, audio_buffer->physical_addr, count, dma_mode);
    dma_start(dma_channel);

    // Program DSP for auto-init playback
    // The DSP will generate an IRQ every HALF_SIZE bytes
    uint8_t cmd;
    uint8_t mode = 0;
    uint16_t sample_count;

    if (current_format.bits == 16) {
        cmd = DSP_CMD_PLAY_16;
        mode = DSP_FORMAT_SIGNED;
        if (current_format.channels == 2) {
            mode |= DSP_FORMAT_STEREO;
        }
        // Count is per-half, in sample frames
        sample_count = (AUDIO_HALF_SIZE / (current_format.channels * 2)) - 1;
    } else {
        cmd = DSP_CMD_PLAY_8;
        mode = DSP_FORMAT_UNSIGNED;
        if (current_format.channels == 2) {
            mode |= DSP_FORMAT_STEREO;
        }
        sample_count = (AUDIO_HALF_SIZE / current_format.channels) - 1;
    }

    // Use auto-init mode (DSP_MODE_AUTO) with FIFO
    dsp_write(cmd | DSP_MODE_FIFO | DSP_MODE_AUTO);
    dsp_write(mode);
    dsp_write((uint8_t)(sample_count & 0xFF));
    dsp_write((uint8_t)(sample_count >> 8));

    auto_init_active = true;
    playing = true;
    current_half = 0;
}

int sb16_write(const void* samples, uint32_t bytes) {
    if (!sb16_present || !audio_buffer || !samples || bytes == 0) {
        return -1;
    }

    // Limit to half buffer size (we fill one half at a time)
    if (bytes > AUDIO_HALF_SIZE) {
        bytes = AUDIO_HALF_SIZE;
    }

    // Find a ready half to fill
    int target_half = -1;

    // Wait for a half to be ready
    uint32_t timeout = 1000000;
    while (timeout > 0) {
        if (half_ready[0]) {
            target_half = 0;
            break;
        }
        if (half_ready[1]) {
            target_half = 1;
            break;
        }
        __asm__ volatile("pause");
        timeout--;
    }

    if (target_half < 0) {
        // Timeout waiting for buffer
        return -1;
    }

    // Copy data to the target half
    uint8_t* dest = (uint8_t*)audio_buffer->virtual_addr + (target_half * AUDIO_HALF_SIZE);
    memcpy(dest, samples, bytes);

    // Zero-pad if less than half buffer
    if (bytes < AUDIO_HALF_SIZE) {
        memset(dest + bytes, 0, AUDIO_HALF_SIZE - bytes);
    }

    // Mark half as filled
    half_ready[target_half] = false;

    // Start playback if not already running
    if (!auto_init_active) {
        // Fill both halves before starting for smooth playback
        if (!half_ready[0] && !half_ready[1]) {
            start_auto_init_playback();
        }
    }

    return (int)bytes;
}

void sb16_start(void) {
    if (!sb16_present) return;

    if (current_format.bits == 16) {
        dsp_write(DSP_CMD_RESUME_16);
    } else {
        dsp_write(DSP_CMD_RESUME_8);
    }
    playing = true;
}

void sb16_stop(void) {
    if (!sb16_present) return;

    // Exit auto-init mode
    if (current_format.bits == 16) {
        dsp_write(DSP_CMD_EXIT_AUTOINIT_16);
        dsp_write(DSP_CMD_STOP_16);
        dma_stop(SB_DEFAULT_DMA_16);
    } else {
        dsp_write(DSP_CMD_EXIT_AUTOINIT_8);
        dsp_write(DSP_CMD_STOP_8);
        dma_stop(SB_DEFAULT_DMA_8);
    }

    playing = false;
    auto_init_active = false;
    half_ready[0] = true;
    half_ready[1] = true;
    current_half = 0;
}

void sb16_set_volume(uint8_t volume) {
    if (!sb16_present) return;

    // Set master volume (left and right channels)
    uint8_t vol = volume >> 3;  // Scale 0-255 to 0-31
    if (vol > 31) vol = 31;
    uint8_t vol_byte = (vol << 3) | (vol >> 2);  // Pack into mixer format

    outb(SB_MIXER_ADDR, MIXER_MASTER_VOL);
    outb(SB_MIXER_DATA, (vol_byte << 4) | vol_byte);

    outb(SB_MIXER_ADDR, MIXER_VOICE_VOL);
    outb(SB_MIXER_DATA, (vol_byte << 4) | vol_byte);
}

bool sb16_is_playing(void) {
    return playing && auto_init_active;
}

void sb16_wait(void) {
    // Wait for both halves to be ready (nothing playing)
    uint32_t timeout = 5000000;
    while (auto_init_active && timeout > 0) {
        if (half_ready[0] && half_ready[1]) {
            break;
        }
        __asm__ volatile("pause");
        timeout--;
    }
    if (timeout == 0) {
        serial_write_string("[SB16] wait timeout - forcing stop\n");
        sb16_stop();
    }
}
