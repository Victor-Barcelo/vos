# Chapter 37: Sound Blaster 16 Audio

This chapter covers VOS's Sound Blaster 16 audio driver, including DMA (Direct Memory Access) programming for hardware audio playback.

## Overview

The Sound Blaster 16 (SB16) is a classic ISA sound card from the early 1990s. Despite its age, it remains the de facto standard for audio in emulators like QEMU and Bochs because:

1. **Well-documented** - The DSP programming interface is thoroughly documented
2. **Emulator support** - QEMU's `-soundhw sb16` provides perfect emulation
3. **Simple protocol** - No PCI enumeration or complex setup required
4. **Educational value** - Demonstrates DMA, IRQs, and hardware I/O

```
+------------------+
|   Application    |
| (MOD/MIDI player)|
+------------------+
        |
        v
+------------------+
|   SB16 Driver    |
| - Format setup   |
| - DSP commands   |
| - IRQ handling   |
+------------------+
        |
        v
+------------------+       +------------------+
|   DMA Engine     | <---> |   Physical RAM   |
| - Channel setup  |       |   (< 16MB)       |
| - Page register  |       +------------------+
+------------------+
        |
        v
+------------------+
|  Sound Blaster   |
|  16 Hardware     |
| - DSP chip       |
| - Mixer chip     |
| - DAC            |
+------------------+
        |
        v
    [Speakers]
```

## Sound Blaster 16 Architecture

### Hardware Components

The SB16 consists of several components:

| Component | Function |
|-----------|----------|
| DSP (Digital Signal Processor) | Main audio processing chip |
| Mixer | Volume control and input selection |
| DAC/ADC | Digital-to-Analog conversion |
| FM Synthesizer | OPL3 music synthesis |
| MPU-401 | MIDI interface |

### I/O Ports

At the default base address 0x220:

```c
// Sound Blaster I/O ports (base 0x220)
#define SB_DSP_RESET      0x226    // Write 1, wait, write 0 to reset
#define SB_DSP_READ       0x22A    // Read data from DSP
#define SB_DSP_WRITE      0x22C    // Write commands/data to DSP
#define SB_DSP_STATUS     0x22C    // Bit 7: 1 = not ready for write
#define SB_DSP_INTR_ACK   0x22E    // Read to acknowledge 8-bit IRQ
#define SB_DSP_INTR_ACK_16 0x22F   // Read to acknowledge 16-bit IRQ
#define SB_MIXER_ADDR     0x224    // Mixer register select
#define SB_MIXER_DATA     0x225    // Mixer data read/write
```

### DSP Commands

The DSP is controlled through a command-based interface:

```c
// DSP Commands
#define DSP_CMD_GET_VERSION    0xE1   // Get DSP version
#define DSP_CMD_SPEAKER_ON     0xD1   // Enable speaker output
#define DSP_CMD_SPEAKER_OFF    0xD3   // Disable speaker output
#define DSP_CMD_SET_SAMPLE_RATE 0x41  // Set output sample rate
#define DSP_CMD_PLAY_8         0xC0   // Start 8-bit playback
#define DSP_CMD_PLAY_16        0xB0   // Start 16-bit playback
#define DSP_CMD_STOP_8         0xD0   // Pause 8-bit playback
#define DSP_CMD_STOP_16        0xD5   // Pause 16-bit playback
#define DSP_CMD_RESUME_8       0xD4   // Resume 8-bit playback
#define DSP_CMD_RESUME_16      0xD6   // Resume 16-bit playback
```

## DMA (Direct Memory Access)

### Why DMA?

Audio playback requires continuous data transfer to the sound card. Without DMA:
- CPU would need to constantly feed bytes to the card
- Any interrupt or delay causes audio glitches
- CPU cannot do other work during playback

With DMA:
- DMA controller transfers data independently
- CPU is free for other tasks
- Hardware handles timing automatically
- Only notified via IRQ when buffer needs refill

### ISA DMA Architecture

The PC has two DMA controllers (8237A):

```
DMA Controller 1 (8-bit, channels 0-3)
    Channel 0: Memory refresh (reserved)
    Channel 1: Sound Blaster 8-bit audio
    Channel 2: Floppy disk
    Channel 3: Available

DMA Controller 2 (16-bit, channels 4-7)
    Channel 4: Cascade to DMA1 (reserved)
    Channel 5: Sound Blaster 16-bit audio
    Channel 6: Available
    Channel 7: Available
```

### DMA Port Addresses

```c
// DMA Controller 1 (8-bit, channels 0-3)
#define DMA1_STATUS       0x08    // Status register
#define DMA1_COMMAND      0x08    // Command register (write)
#define DMA1_REQUEST      0x09    // Request register
#define DMA1_SINGLE_MASK  0x0A    // Single channel mask
#define DMA1_MODE         0x0B    // Mode register
#define DMA1_CLEAR_FF     0x0C    // Clear flip-flop
#define DMA1_MASTER_CLEAR 0x0D    // Master clear (reset)
#define DMA1_MULTI_MASK   0x0F    // Multi-channel mask

// DMA Controller 2 (16-bit, channels 4-7)
#define DMA2_STATUS       0xD0
#define DMA2_COMMAND      0xD0
#define DMA2_REQUEST      0xD2
#define DMA2_SINGLE_MASK  0xD4
#define DMA2_MODE         0xD6
#define DMA2_CLEAR_FF     0xD8
#define DMA2_MASTER_CLEAR 0xDA
#define DMA2_MULTI_MASK   0xDE
```

### Page Registers

DMA can only address 64KB at a time. Page registers provide the upper address bits:

```c
// Page registers for each DMA channel
static const uint8_t dma_page_ports[8] = {
    0x87,  // Channel 0
    0x83,  // Channel 1 (SB16 8-bit)
    0x81,  // Channel 2
    0x82,  // Channel 3
    0x8F,  // Channel 4 (cascade)
    0x8B,  // Channel 5 (SB16 16-bit)
    0x89,  // Channel 6
    0x8A   // Channel 7
};
```

### DMA Constraints

ISA DMA has important limitations:

1. **16MB limit** - Physical address must be below 16MB
2. **64KB boundary** - Transfer cannot cross a 64KB boundary
3. **Alignment** - 16-bit DMA uses word addresses

```c
// DMA buffer structure
typedef struct {
    void* virtual_addr;      // Kernel-accessible address
    uint32_t physical_addr;  // Physical address (< 16MB)
    uint32_t size;           // Buffer size
} dma_buffer_t;

// Static DMA buffer in kernel BSS (guaranteed < 16MB)
#define STATIC_DMA_BUFFER_SIZE 65536
static uint8_t static_dma_buffer[STATIC_DMA_BUFFER_SIZE]
    __attribute__((aligned(65536)));
```

## VOS Sound Blaster Driver Implementation

### Driver Initialization

```c
int sb16_init(void) {
    // 1. Reset the DSP
    if (!dsp_reset()) {
        return -1;  // Card not present
    }

    // 2. Get and verify DSP version
    dsp_write(DSP_CMD_GET_VERSION);
    uint8_t major = dsp_read();
    uint8_t minor = dsp_read();

    // SB16 requires version 4.xx or higher
    if (major < 4) {
        return -1;  // Not an SB16
    }

    // 3. Allocate DMA buffer
    audio_buffer = dma_alloc_buffer(AUDIO_BUFFER_SIZE);
    if (!audio_buffer) {
        return -1;
    }

    // 4. Register IRQ handler
    irq_register_handler(SB_DEFAULT_IRQ, sb16_irq_handler);

    // 5. Configure mixer for IRQ and DMA
    outb(SB_MIXER_ADDR, MIXER_INT_SETUP);
    outb(SB_MIXER_DATA, 0x02);  // IRQ 5

    outb(SB_MIXER_ADDR, MIXER_DMA_SETUP);
    outb(SB_MIXER_DATA, 0x22);  // DMA 1 and 5

    // 6. Turn speaker on
    dsp_write(DSP_CMD_SPEAKER_ON);

    return 0;
}
```

### DSP Reset Procedure

The DSP requires a specific reset sequence:

```c
static bool dsp_reset(void) {
    // 1. Write 1 to reset port
    outb(SB_DSP_RESET, 1);

    // 2. Wait at least 3 microseconds
    for (int i = 0; i < 100; i++) {
        inb(SB_DSP_RESET);  // Small delay
    }

    // 3. Write 0 to reset port
    outb(SB_DSP_RESET, 0);

    // 4. Wait for ready signal (0xAA)
    for (int i = 0; i < 100; i++) {
        if (dsp_read_ready()) {
            uint8_t val = inb(SB_DSP_READ);
            if (val == 0xAA) {
                return true;  // Success!
            }
        }
    }

    return false;  // Reset failed
}
```

### Setting Audio Format

```c
int sb16_set_format(uint32_t sample_rate, uint8_t bits, uint8_t channels) {
    // Validate parameters
    if (bits != 8 && bits != 16) return -1;
    if (channels != 1 && channels != 2) return -1;
    if (sample_rate < 5000 || sample_rate > 44100) return -1;

    // Set sample rate using SB16 time constant
    dsp_write(DSP_CMD_SET_SAMPLE_RATE);
    dsp_write((uint8_t)(sample_rate >> 8));    // High byte
    dsp_write((uint8_t)(sample_rate & 0xFF));  // Low byte

    current_format.sample_rate = sample_rate;
    current_format.bits = bits;
    current_format.channels = channels;

    return 0;
}
```

### Playing Audio

```c
int sb16_write(const void* samples, uint32_t bytes) {
    // 1. Wait for buffer to be ready
    while (!buffer_ready) {
        __asm__ volatile("pause");
    }

    // 2. Copy samples to DMA buffer
    memcpy(audio_buffer->virtual_addr, samples, bytes);
    buffer_ready = false;

    // 3. Set up DMA transfer
    uint8_t dma_channel = (current_format.bits == 16)
        ? SB_DEFAULT_DMA_16   // Channel 5
        : SB_DEFAULT_DMA_8;   // Channel 1

    dma_setup_transfer(dma_channel,
                       audio_buffer->physical_addr,
                       bytes - 1,  // DMA counts from 0
                       DMA_MODE_SINGLE | DMA_MODE_READ);
    dma_start(dma_channel);

    // 4. Program DSP for playback
    if (current_format.bits == 16) {
        uint8_t mode = DSP_FORMAT_SIGNED;
        if (current_format.channels == 2) {
            mode |= DSP_FORMAT_STEREO;
        }

        uint16_t sample_count = (bytes / 2) - 1;

        dsp_write(DSP_CMD_PLAY_16 | DSP_MODE_FIFO);
        dsp_write(mode);
        dsp_write((uint8_t)(sample_count & 0xFF));
        dsp_write((uint8_t)(sample_count >> 8));
    }

    return bytes;
}
```

### DMA Setup

```c
void dma_setup_transfer(uint8_t channel, uint32_t phys_addr,
                        uint16_t count, uint8_t mode) {
    bool is_16bit = (channel >= 4);

    // 1. Disable the channel
    dma_stop(channel);

    // 2. Clear flip-flop (addresses are sent as two bytes)
    if (is_16bit) {
        outb(DMA2_CLEAR_FF, 0xFF);
    } else {
        outb(DMA1_CLEAR_FF, 0xFF);
    }

    // 3. Set the mode
    uint8_t mode_val = (channel & 0x03) | mode;
    if (is_16bit) {
        outb(DMA2_MODE, mode_val);
    } else {
        outb(DMA1_MODE, mode_val);
    }

    // 4. Set address (low byte, then high byte)
    uint32_t addr = is_16bit ? (phys_addr >> 1) : phys_addr;
    outb(dma_addr_ports[channel], (uint8_t)(addr & 0xFF));
    outb(dma_addr_ports[channel], (uint8_t)((addr >> 8) & 0xFF));

    // 5. Set page register (bits 16-23)
    outb(dma_page_ports[channel], (uint8_t)((phys_addr >> 16) & 0xFF));

    // 6. Set count (low byte, then high byte)
    uint16_t cnt = is_16bit ? (count >> 1) : count;
    outb(dma_count_ports[channel], (uint8_t)(cnt & 0xFF));
    outb(dma_count_ports[channel], (uint8_t)((cnt >> 8) & 0xFF));
}
```

### IRQ Handler

```c
static void sb16_irq_handler(interrupt_frame_t* frame) {
    (void)frame;

    irq_count++;
    buffer_ready = true;  // Signal that buffer can be refilled

    // Acknowledge the interrupt (required!)
    if (current_format.bits == 16) {
        inb(SB_DSP_INTR_ACK_16);
    } else {
        inb(SB_DSP_INTR_ACK);
    }
}
```

## Mixer Control

The SB16 mixer controls volume levels:

```c
// Mixer registers
#define MIXER_MASTER_VOL   0x22   // Master volume
#define MIXER_VOICE_VOL    0x04   // Voice/WAV volume
#define MIXER_INT_SETUP    0x80   // IRQ selection
#define MIXER_DMA_SETUP    0x81   // DMA selection

void sb16_set_volume(uint8_t volume) {
    // Scale 0-255 to 0-31 (5-bit resolution)
    uint8_t vol = volume >> 3;
    if (vol > 31) vol = 31;
    uint8_t vol_byte = (vol << 3) | (vol >> 2);

    // Set master volume (left and right)
    outb(SB_MIXER_ADDR, MIXER_MASTER_VOL);
    outb(SB_MIXER_DATA, (vol_byte << 4) | vol_byte);

    // Set voice volume
    outb(SB_MIXER_ADDR, MIXER_VOICE_VOL);
    outb(SB_MIXER_DATA, (vol_byte << 4) | vol_byte);
}
```

## Audio Formats

### Supported Formats

| Format | Sample Rate | Bits | Channels |
|--------|-------------|------|----------|
| Low quality | 11025 Hz | 8 | Mono |
| Medium quality | 22050 Hz | 8/16 | Mono/Stereo |
| CD quality | 44100 Hz | 16 | Stereo |

### Audio Data Layout

```
8-bit Mono:     [S0] [S1] [S2] ...
8-bit Stereo:   [L0] [R0] [L1] [R1] ...
16-bit Mono:    [S0_L] [S0_H] [S1_L] [S1_H] ...
16-bit Stereo:  [L0_L] [L0_H] [R0_L] [R0_H] ...

S = Sample, L = Left, R = Right
_L = Low byte, _H = High byte (little-endian)
```

### Sample Format

```c
// 8-bit: Unsigned, centered at 128
// Silence = 128, Max negative = 0, Max positive = 255

// 16-bit: Signed, centered at 0
// Silence = 0, Max negative = -32768, Max positive = 32767

typedef struct sb16_format {
    uint32_t sample_rate;  // 5000-44100 Hz
    uint8_t bits;          // 8 or 16
    uint8_t channels;      // 1 (mono) or 2 (stereo)
} sb16_format_t;
```

## VOS Audio Applications

### MOD Player

VOS includes `modplay` for playing MOD/S3M/XM tracker music:

```bash
modplay /disk/music/song.mod
```

Features:
- Supports MOD, S3M, XM, IT formats
- Real-time mixing to 44100Hz stereo
- Uses micromod library for decoding

### MIDI Player

VOS includes `midiplay` for MIDI files:

```bash
midiplay /disk/music/song.mid
```

Features:
- General MIDI compatible
- Uses TinySoundFont for synthesis
- SF2 soundfont support

### Beep Utility

Simple PC speaker beeps (uses PIT, not SB16):

```bash
beep 440 500   # 440 Hz for 500ms
```

## Testing in QEMU

Enable Sound Blaster 16 emulation:

```bash
qemu-system-i386 -cdrom vos.iso \
    -audiodev pa,id=audio0 \
    -machine pcspk-audiodev=audio0 \
    -device sb16,audiodev=audio0
```

Or with SDL audio backend:

```bash
qemu-system-i386 -cdrom vos.iso \
    -audiodev sdl,id=audio0 \
    -device sb16,audiodev=audio0
```

## Troubleshooting

### "DSP reset failed"

- SB16 not enabled in QEMU
- Wrong I/O base address
- IRQ conflict

### No audio output

1. Check QEMU audio backend is working
2. Verify mixer volume settings
3. Check DMA buffer is in low memory
4. Verify IRQ is unmasked

### Audio glitches/pops

- DMA buffer too small
- Buffer refill too slow
- IRQ handling latency

## Summary

The Sound Blaster 16 driver demonstrates:

1. **ISA device programming** - Port I/O, reset sequences
2. **DMA transfers** - Automatic memory-to-device transfers
3. **IRQ handling** - Buffer completion notification
4. **Audio concepts** - Sample rates, bit depths, channels

Despite being 30-year-old hardware, SB16 remains an excellent teaching platform for low-level audio programming.

---

*Previous: [Chapter 36: Emoji Support](36_emoji.md)*
*Next: [Chapter 38: Font System and Themes](38_fonts.md)*
