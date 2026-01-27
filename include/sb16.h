#ifndef SB16_H
#define SB16_H

#include "types.h"

// Sound Blaster 16 I/O ports (base = 0x220)
#define SB_BASE             0x220
#define SB_MIXER_ADDR       (SB_BASE + 0x04)  // Mixer address port
#define SB_MIXER_DATA       (SB_BASE + 0x05)  // Mixer data port
#define SB_DSP_RESET        (SB_BASE + 0x06)  // DSP reset port
#define SB_DSP_READ         (SB_BASE + 0x0A)  // DSP read port
#define SB_DSP_WRITE        (SB_BASE + 0x0C)  // DSP write port (write command/data)
#define SB_DSP_STATUS       (SB_BASE + 0x0C)  // DSP write status (bit 7 = ready)
#define SB_DSP_INTR_ACK     (SB_BASE + 0x0E)  // DSP interrupt acknowledge (8-bit)
#define SB_DSP_INTR_ACK_16  (SB_BASE + 0x0F)  // DSP interrupt acknowledge (16-bit)

// DSP commands
#define DSP_CMD_SET_TIME_CONST  0x40  // Set time constant (8-bit)
#define DSP_CMD_SET_SAMPLE_RATE 0x41  // Set sample rate (16-bit, SB16)
#define DSP_CMD_SPEAKER_ON      0xD1  // Turn speaker on
#define DSP_CMD_SPEAKER_OFF     0xD3  // Turn speaker off
#define DSP_CMD_STOP_8          0xD0  // Stop 8-bit DMA
#define DSP_CMD_RESUME_8        0xD4  // Resume 8-bit DMA
#define DSP_CMD_STOP_16         0xD5  // Stop 16-bit DMA
#define DSP_CMD_RESUME_16       0xD6  // Resume 16-bit DMA
#define DSP_CMD_GET_VERSION     0xE1  // Get DSP version
#define DSP_CMD_EXIT_AUTOINIT_16 0xD9 // Exit auto-init 16-bit mode
#define DSP_CMD_EXIT_AUTOINIT_8  0xDA // Exit auto-init 8-bit mode

// 8-bit playback commands (SB Pro and earlier)
#define DSP_CMD_8BIT_OUT_AUTO   0x1C  // 8-bit auto-init DMA output
#define DSP_CMD_8BIT_OUT_SINGLE 0x14  // 8-bit single-cycle DMA output

// 16-bit playback commands (SB16)
#define DSP_CMD_PLAY_16         0xB0  // 16-bit DMA mode
#define DSP_CMD_PLAY_8          0xC0  // 8-bit DMA mode

// Mode bits for DSP_CMD_PLAY_16/8
#define DSP_MODE_FIFO           0x02  // FIFO on
#define DSP_MODE_AUTO           0x04  // Auto-initialize mode
#define DSP_MODE_INPUT          0x08  // Input (recording) mode

// Format bits
#define DSP_FORMAT_MONO         0x00
#define DSP_FORMAT_STEREO       0x20
#define DSP_FORMAT_UNSIGNED     0x00
#define DSP_FORMAT_SIGNED       0x10

// Mixer registers
#define MIXER_MASTER_VOL        0x22  // Master volume
#define MIXER_VOICE_VOL         0x04  // Voice/DAC volume
#define MIXER_INT_SETUP         0x80  // Interrupt setup
#define MIXER_DMA_SETUP         0x81  // DMA setup
#define MIXER_IRQ_STATUS        0x82  // IRQ status

// Default settings
#define SB_DEFAULT_IRQ          5
#define SB_DEFAULT_DMA_8        1
#define SB_DEFAULT_DMA_16       5

// Audio format structure
typedef struct sb16_format {
    uint32_t sample_rate;  // e.g., 44100
    uint8_t bits;          // 8 or 16
    uint8_t channels;      // 1 (mono) or 2 (stereo)
} sb16_format_t;

// Initialize Sound Blaster 16 driver
// Returns 0 on success, -1 on failure
int sb16_init(void);

// Check if SB16 was detected during init
bool sb16_detected(void);

// Get DSP version (major in high byte, minor in low byte)
uint16_t sb16_get_version(void);

// Set audio format
// Returns 0 on success, -1 on failure
int sb16_set_format(uint32_t sample_rate, uint8_t bits, uint8_t channels);

// Write audio samples (blocking)
// samples: pointer to PCM data
// bytes: number of bytes to write
// Returns number of bytes written, or -1 on error
int sb16_write(const void* samples, uint32_t bytes);

// Start playback (called automatically by sb16_write)
void sb16_start(void);

// Stop playback
void sb16_stop(void);

// Set master volume (0-255)
void sb16_set_volume(uint8_t volume);

// Check if playback is active
bool sb16_is_playing(void);

// Wait for current buffer to finish playing
void sb16_wait(void);

#endif // SB16_H
