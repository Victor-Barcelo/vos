/*
 * SDL2 Audio Subsystem for VOS
 *
 * Minimal SDL2 audio shim that maps to VOS audio syscalls.
 * Since VOS has no threads, audio is pumped manually via SDL_PumpAudio().
 */

#ifndef SDL_audio_h_
#define SDL_audio_h_

#include "SDL_stdinc.h"

/* Audio format flags */
typedef Uint16 SDL_AudioFormat;

#define AUDIO_U8        0x0008  /* Unsigned 8-bit samples */
#define AUDIO_S8        0x8008  /* Signed 8-bit samples */
#define AUDIO_U16LSB    0x0010  /* Unsigned 16-bit samples (little-endian) */
#define AUDIO_S16LSB    0x8010  /* Signed 16-bit samples (little-endian) */
#define AUDIO_U16MSB    0x1010  /* Unsigned 16-bit samples (big-endian) */
#define AUDIO_S16MSB    0x9010  /* Signed 16-bit samples (big-endian) */
#define AUDIO_S16       AUDIO_S16LSB  /* Default 16-bit format */
#define AUDIO_U16       AUDIO_U16LSB

/* System native byte order (little-endian on x86/VOS) */
#define AUDIO_U16SYS    AUDIO_U16LSB
#define AUDIO_S16SYS    AUDIO_S16LSB

/* Macros for extracting format info */
#define SDL_AUDIO_BITSIZE(x)    ((x) & 0xFF)
#define SDL_AUDIO_ISSIGNED(x)   ((x) & 0x8000)
#define SDL_AUDIO_ISBIGENDIAN(x) ((x) & 0x1000)
#define SDL_AUDIO_ISINT(x)      (!((x) & 0x0100))
#define SDL_AUDIO_ISFLOAT(x)    ((x) & 0x0100)

/* Audio callback function type */
typedef void (*SDL_AudioCallback)(void *userdata, Uint8 *stream, int len);

/* Audio specification structure */
typedef struct SDL_AudioSpec {
    int freq;                   /* DSP frequency (samples per second) */
    SDL_AudioFormat format;     /* Audio data format */
    Uint8 channels;             /* Number of channels: 1 mono, 2 stereo */
    Uint8 silence;              /* Audio buffer silence value (calculated) */
    Uint16 samples;             /* Audio buffer size in samples (power of 2) */
    Uint16 padding;             /* Padding for alignment */
    Uint32 size;                /* Audio buffer size in bytes (calculated) */
    SDL_AudioCallback callback; /* Callback that feeds the audio device */
    void *userdata;             /* User data passed to callback */
} SDL_AudioSpec;

/* Audio device ID type (for device-based API, not fully implemented) */
typedef Uint32 SDL_AudioDeviceID;

/* Audio status values */
typedef enum {
    SDL_AUDIO_STOPPED = 0,
    SDL_AUDIO_PLAYING,
    SDL_AUDIO_PAUSED
} SDL_AudioStatus;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Open the audio device with desired parameters.
 *
 * @param desired  Desired audio specification
 * @param obtained If non-NULL, filled with actual audio specification
 * @return 0 on success, -1 on error
 */
int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);

/*
 * Close the audio device.
 */
void SDL_CloseAudio(void);

/*
 * Pause or unpause audio playback.
 *
 * @param pause_on 1 to pause, 0 to unpause
 */
void SDL_PauseAudio(int pause_on);

/*
 * Lock the audio device (no-op on VOS, no threads).
 */
void SDL_LockAudio(void);

/*
 * Unlock the audio device (no-op on VOS, no threads).
 */
void SDL_UnlockAudio(void);

/*
 * Get current audio status.
 *
 * @return SDL_AUDIO_STOPPED, SDL_AUDIO_PLAYING, or SDL_AUDIO_PAUSED
 */
SDL_AudioStatus SDL_GetAudioStatus(void);

/*
 * VOS Extension: Pump audio data to the device.
 *
 * Since VOS has no threads, the application must call this function
 * regularly (e.g., in the main loop) to feed audio data to the device.
 *
 * This function calls the user's callback to get audio samples,
 * then writes them to the audio device via sys_audio_write().
 *
 * Call frequency: At least (sample_rate / samples) times per second
 * to avoid audio underruns.
 */
void SDL_PumpAudio(void);

/*
 * Mix audio data (simple add with clipping).
 *
 * @param dst    Destination buffer
 * @param src    Source buffer
 * @param len    Number of bytes to mix
 * @param volume Volume level (0-128, 128 = full volume)
 */
void SDL_MixAudio(Uint8 *dst, const Uint8 *src, Uint32 len, int volume);

/*
 * Get the last audio error message.
 *
 * @return Error string (static buffer, do not free)
 */
const char *SDL_GetAudioError(void);

#ifdef __cplusplus
}
#endif

#endif /* SDL_audio_h_ */
