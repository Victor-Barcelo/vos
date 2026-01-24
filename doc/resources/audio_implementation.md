# Audio Implementation Roadmap for VOS

This document outlines options for adding audio support to VOS.

---

## Current State

VOS has no audio support. Options range from simple (PC speaker) to complex (Sound Blaster, Intel HDA).

---

## Option 1: PC Speaker (Easiest)

### Overview
The PC speaker can produce simple square wave tones using the PIT (Programmable Interval Timer).

### Hardware Details
- Uses PIT Channel 2
- Port 0x42: Frequency divisor
- Port 0x43: Control register
- Port 0x61: Speaker enable

### Kernel Implementation

```c
// kernel/speaker.c

#include "io.h"
#include "timer.h"

#define PIT_FREQ 1193180
#define SPEAKER_PORT 0x61
#define PIT_CMD 0x43
#define PIT_CH2 0x42

void speaker_on(uint32_t frequency) {
    uint32_t divisor = PIT_FREQ / frequency;

    // Configure PIT channel 2 for square wave
    outb(PIT_CMD, 0xB6);  // Channel 2, lobyte/hibyte, square wave

    // Set frequency
    outb(PIT_CH2, divisor & 0xFF);
    outb(PIT_CH2, (divisor >> 8) & 0xFF);

    // Enable speaker
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp | 0x03);
}

void speaker_off(void) {
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp & 0xFC);
}

void beep(uint32_t frequency, uint32_t duration_ms) {
    speaker_on(frequency);
    sleep_ms(duration_ms);
    speaker_off();
}
```

### User-Space API
```c
// New syscalls
#define SYS_SPEAKER_ON   90
#define SYS_SPEAKER_OFF  91
#define SYS_BEEP         92

// Usage
syscall1(SYS_SPEAKER_ON, 440);   // A4 note
syscall0(SYS_SPEAKER_OFF);
syscall2(SYS_BEEP, 880, 100);    // A5 for 100ms
```

### Musical Notes
```c
// Note frequencies (Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

// Example: Play scale
int scale[] = {NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4,
               NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5};
for (int i = 0; i < 8; i++) {
    beep(scale[i], 200);
    sleep_ms(50);
}
```

### Advantages
- Extremely simple
- Works on all x86 systems
- No DMA or interrupts needed

### Disadvantages
- Single channel only
- No amplitude control
- Square waves only (harsh sound)
- Blocks CPU during playback

---

## Option 2: Sound Blaster 16 (Moderate)

### Overview
The Sound Blaster 16 is a classic ISA sound card with good QEMU support. Supports 8/16-bit PCM audio.

### Hardware Details
- Base address: 0x220 (default)
- IRQ: 5 or 7
- DMA: Channel 1 (8-bit) or 5 (16-bit)
- Sample rates: 5000-44100 Hz

### Port Map
| Port | Function |
|------|----------|
| base+0x4 | Mixer address |
| base+0x5 | Mixer data |
| base+0x6 | Reset |
| base+0xA | Read data |
| base+0xC | Write command/data |
| base+0xE | Read status |

### Kernel Implementation

```c
// kernel/sb16.c

#define SB_BASE     0x220
#define SB_RESET    (SB_BASE + 0x06)
#define SB_READ     (SB_BASE + 0x0A)
#define SB_WRITE    (SB_BASE + 0x0C)
#define SB_STATUS   (SB_BASE + 0x0E)

#define SB_IRQ      5
#define SB_DMA      1

// DMA buffer (must be < 64KB and not cross 64KB boundary)
static uint8_t dma_buffer[65536] __attribute__((aligned(65536)));
static volatile int playing = 0;

int sb16_init(void) {
    // Reset DSP
    outb(SB_RESET, 1);
    for (int i = 0; i < 100; i++) io_wait();
    outb(SB_RESET, 0);

    // Wait for ready (0xAA)
    for (int i = 0; i < 100; i++) {
        if (inb(SB_STATUS) & 0x80) {
            if (inb(SB_READ) == 0xAA) {
                return 0;  // Success
            }
        }
        io_wait();
    }
    return -1;  // Failed
}

void sb16_write(uint8_t value) {
    while (inb(SB_STATUS) & 0x80);
    outb(SB_WRITE, value);
}

void sb16_set_sample_rate(uint16_t rate) {
    sb16_write(0x41);  // Set output rate
    sb16_write(rate >> 8);
    sb16_write(rate & 0xFF);
}

void sb16_play(uint8_t *data, size_t len, uint16_t rate) {
    // Copy to DMA buffer
    memcpy(dma_buffer, data, len);

    // Setup DMA channel
    dma_setup(SB_DMA, dma_buffer, len, DMA_MODE_SINGLE | DMA_MODE_READ);

    // Set sample rate
    sb16_set_sample_rate(rate);

    // Start playback (8-bit mono)
    sb16_write(0xC0);  // 8-bit output
    sb16_write(0x00);  // Mono, unsigned
    sb16_write((len - 1) & 0xFF);
    sb16_write((len - 1) >> 8);

    playing = 1;
}

// IRQ5 handler
void sb16_irq(void) {
    inb(SB_STATUS);  // Acknowledge
    playing = 0;
    pic_eoi(SB_IRQ);
}
```

### DMA Setup
```c
// kernel/dma.c

#define DMA_ADDR_0   0x00
#define DMA_COUNT_0  0x01
#define DMA_PAGE_0   0x87
#define DMA_MASK     0x0A
#define DMA_MODE     0x0B
#define DMA_FLIPFLOP 0x0C

void dma_setup(int channel, void *addr, size_t len, int mode) {
    uint32_t phys = (uint32_t)addr;

    // Disable DMA channel
    outb(DMA_MASK, channel | 0x04);

    // Reset flip-flop
    outb(DMA_FLIPFLOP, 0);

    // Set mode
    outb(DMA_MODE, mode | channel);

    // Set address
    outb(DMA_ADDR_0 + (channel * 2), phys & 0xFF);
    outb(DMA_ADDR_0 + (channel * 2), (phys >> 8) & 0xFF);

    // Set page
    outb(DMA_PAGE_0, (phys >> 16) & 0xFF);

    // Set count
    outb(DMA_COUNT_0 + (channel * 2), (len - 1) & 0xFF);
    outb(DMA_COUNT_0 + (channel * 2), ((len - 1) >> 8) & 0xFF);

    // Enable DMA channel
    outb(DMA_MASK, channel);
}
```

### User-Space API
```c
// Audio syscalls
#define SYS_AUDIO_INIT       93
#define SYS_AUDIO_PLAY       94
#define SYS_AUDIO_STOP       95
#define SYS_AUDIO_STATUS     96

// Higher-level wrapper
int audio_play_wav(const char *filename) {
    // Parse WAV header
    // Setup playback
    // Return immediately (non-blocking)
}
```

### Advantages
- Real PCM audio
- Good quality
- Well-documented
- QEMU supports it

### Disadvantages
- DMA setup is complex
- ISA bus limitations
- Only in emulators typically

---

## Option 3: Intel AC'97 (Modern but Complex)

### Overview
AC'97 is found in many late 90s/2000s systems. More complex but better quality.

### Features
- 16-bit stereo audio
- Up to 48kHz sample rate
- Multiple streams
- Hardware mixing

### Registers (via PCI)
| Register | Function |
|----------|----------|
| NABM BAR | Bus Master registers |
| NAM BAR | Mixer registers |
| PCMOUT | PCM output buffer descriptors |

### Complexity
- Requires PCI enumeration
- Scatter-gather DMA
- Ring buffer management
- ~500 lines of code

---

## Option 4: Intel HD Audio (Most Complex)

### Overview
Modern audio codec found in most current systems. Very complex but best quality.

### Features
- Multiple streams
- 24-bit audio
- Up to 192kHz
- Hardware effects

### Complexity
- Full PCI driver
- CORB/RIRB command queues
- Codec discovery and configuration
- ~2000+ lines of code

---

## Audio Libraries for User Space

### dr_libs (Recommended)
```c
// dr_wav.h - WAV loading
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

drwav wav;
if (drwav_init_file(&wav, "sound.wav", NULL)) {
    int16_t *samples = malloc(wav.totalPCMFrameCount * wav.channels * 2);
    drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, samples);
    audio_play(samples, wav.totalPCMFrameCount, wav.sampleRate);
    drwav_uninit(&wav);
}
```

### stb_vorbis (OGG Vorbis)
```c
#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

int channels, sample_rate;
short *output;
int samples = stb_vorbis_decode_filename("music.ogg",
    &channels, &sample_rate, &output);
```

### Audio Formats
| Library | Format | Size |
|---------|--------|------|
| dr_wav | WAV | Single header |
| dr_mp3 | MP3 | Single header |
| dr_flac | FLAC | Single header |
| stb_vorbis | OGG | Single header |

---

## QEMU Audio Options

### Enable Sound Blaster 16
```bash
qemu-system-i386 -cdrom vos.iso \
    -device sb16,audiodev=audio0 \
    -audiodev pa,id=audio0    # PulseAudio
    # or: -audiodev sdl,id=audio0
```

### Enable AC'97
```bash
qemu-system-i386 -cdrom vos.iso \
    -device AC97,audiodev=audio0 \
    -audiodev pa,id=audio0
```

### Enable Intel HDA
```bash
qemu-system-i386 -cdrom vos.iso \
    -device intel-hda -device hda-duplex,audiodev=audio0 \
    -audiodev pa,id=audio0
```

---

## Software Synthesis

### Simple Waveforms
```c
// Generate sine wave
void generate_sine(int16_t *buffer, int samples, int freq, int sample_rate) {
    for (int i = 0; i < samples; i++) {
        float t = (float)i / sample_rate;
        buffer[i] = (int16_t)(32767 * sin(2 * M_PI * freq * t));
    }
}

// Generate square wave
void generate_square(int16_t *buffer, int samples, int freq, int sample_rate) {
    int period = sample_rate / freq;
    for (int i = 0; i < samples; i++) {
        buffer[i] = (i % period < period / 2) ? 32767 : -32768;
    }
}
```

### Simple Mixer
```c
// Mix two buffers
void mix(int16_t *out, int16_t *a, int16_t *b, int samples) {
    for (int i = 0; i < samples; i++) {
        int32_t mixed = (int32_t)a[i] + (int32_t)b[i];
        // Clamp to prevent overflow
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;
        out[i] = (int16_t)mixed;
    }
}
```

---

## Recommended Implementation Path

### Phase 1: PC Speaker
1. Add speaker driver (~50 lines)
2. Add syscalls for beep/tone
3. Create simple music player
4. **Immediate value, low risk**

### Phase 2: Sound Blaster 16
1. Add DMA driver (~100 lines)
2. Add SB16 driver (~200 lines)
3. Add audio syscalls
4. Port dr_wav for WAV playback
5. **Good quality, moderate effort**

### Phase 3: AC'97 (Optional)
1. Requires PCI driver first
2. Add AC'97 driver (~500 lines)
3. Better quality and mixing
4. **Only if SB16 insufficient**

---

## Complexity Estimates

| Option | Kernel Lines | User Lines | Difficulty |
|--------|--------------|------------|------------|
| PC Speaker | ~50 | ~20 | Easy |
| Sound Blaster 16 | ~300 | ~100 | Medium |
| AC'97 | ~500 | ~100 | Medium-Hard |
| Intel HDA | ~2000 | ~100 | Hard |

---

## See Also

- [Chapter 32: Future Enhancements](../book/32_future.md)
- [data_formats.md](data_formats.md) - Audio file format libraries
- [game_resources.md](game_resources.md) - Game sound effects
