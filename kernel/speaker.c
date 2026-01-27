#include "speaker.h"
#include "io.h"
#include "timer.h"

// PC Speaker uses PIT Channel 2 for tone generation
#define PIT_BASE_HZ       1193182u
#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND       0x43
#define SPEAKER_PORT      0x61

// PIT command: Channel 2, access mode lobyte/hibyte, mode 3 (square wave)
#define PIT_CMD_CHANNEL2_MODE3 0xB6

void speaker_init(void) {
    // Ensure speaker is off at startup
    speaker_stop();
}

void speaker_play(uint32_t frequency) {
    if (frequency == 0) {
        speaker_stop();
        return;
    }

    // Clamp frequency to reasonable range
    if (frequency < 20) {
        frequency = 20;
    }
    if (frequency > 20000) {
        frequency = 20000;
    }

    // Calculate PIT divisor
    uint32_t divisor = PIT_BASE_HZ / frequency;
    if (divisor == 0) {
        divisor = 1;
    }
    if (divisor > 65535) {
        divisor = 65535;
    }

    // Configure PIT Channel 2 for square wave generation
    outb(PIT_COMMAND, PIT_CMD_CHANNEL2_MODE3);
    outb(PIT_CHANNEL2_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL2_DATA, (uint8_t)((divisor >> 8) & 0xFF));

    // Enable speaker: set bits 0 (gate) and 1 (speaker data)
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp | 0x03);
}

void speaker_stop(void) {
    // Disable speaker: clear bits 0 and 1
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp & ~0x03);
}

void speaker_beep(uint32_t frequency, uint32_t duration_ms) {
    speaker_play(frequency);
    timer_sleep_ms(duration_ms);
    speaker_stop();
}
