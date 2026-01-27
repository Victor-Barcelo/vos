#ifndef SPEAKER_H
#define SPEAKER_H

#include "types.h"

// Initialize the PC speaker (no-op for now, but keeps driver pattern consistent)
void speaker_init(void);

// Play a tone at the specified frequency (Hz)
// Frequency should be between 20 and 20000 Hz for audible range
void speaker_play(uint32_t frequency);

// Stop any currently playing tone
void speaker_stop(void);

// Play a beep for a specified duration in milliseconds
void speaker_beep(uint32_t frequency, uint32_t duration_ms);

#endif
