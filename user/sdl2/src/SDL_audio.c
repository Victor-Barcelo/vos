/*
 * SDL2 Audio Subsystem Implementation for VOS
 *
 * Maps SDL2 audio API to VOS audio syscalls:
 * - sys_audio_open(sample_rate, bits, channels)
 * - sys_audio_write(handle, samples, bytes)
 * - sys_audio_close(handle)
 *
 * Since VOS has no threading support, audio must be pumped manually
 * by calling SDL_PumpAudio() regularly in the application's main loop.
 */

/*
 * When building for VOS, include paths are set up as:
 * -I../include for SDL2 headers
 * -I../../ for syscall.h
 */
#include "SDL2/SDL_audio.h"
#include "syscall.h"
#include <string.h>

/* Internal state */
static struct {
    int handle;                     /* VOS audio device handle (-1 if closed) */
    SDL_AudioSpec spec;             /* Current audio specification */
    SDL_AudioStatus status;         /* Current playback status */
    Uint8 *buffer;                  /* Audio buffer for callback */
    Uint32 buffer_size;             /* Size of audio buffer in bytes */
    int initialized;                /* Whether audio subsystem is initialized */
    const char *error;              /* Last error message */
} audio_state = {
    .handle = -1,
    .status = SDL_AUDIO_STOPPED,
    .buffer = NULL,
    .buffer_size = 0,
    .initialized = 0,
    .error = NULL
};

/* Static buffer for audio data (avoid dynamic allocation) */
#define AUDIO_BUFFER_MAX 4096
static Uint8 static_audio_buffer[AUDIO_BUFFER_MAX];

/*
 * Calculate the silence value for a given audio format.
 */
static Uint8 calculate_silence(SDL_AudioFormat format)
{
    /* Unsigned formats have silence at 0x80, signed at 0x00 */
    if (SDL_AUDIO_ISSIGNED(format)) {
        return 0x00;
    } else {
        return 0x80;
    }
}

/*
 * Extract bits per sample from SDL audio format.
 */
static Uint8 format_to_bits(SDL_AudioFormat format)
{
    return (Uint8)SDL_AUDIO_BITSIZE(format);
}

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
    int handle;
    Uint8 bits;
    Uint32 buffer_size;

    if (!desired) {
        audio_state.error = "SDL_OpenAudio: desired spec is NULL";
        return -1;
    }

    /* Close any existing audio device */
    if (audio_state.handle >= 0) {
        SDL_CloseAudio();
    }

    /* Extract format parameters */
    bits = format_to_bits(desired->format);

    /* Validate parameters */
    if (bits != 8 && bits != 16) {
        audio_state.error = "SDL_OpenAudio: only 8-bit and 16-bit audio supported";
        return -1;
    }

    if (desired->channels != 1 && desired->channels != 2) {
        audio_state.error = "SDL_OpenAudio: only mono and stereo supported";
        return -1;
    }

    if (desired->freq < 8000 || desired->freq > 48000) {
        audio_state.error = "SDL_OpenAudio: sample rate must be 8000-48000 Hz";
        return -1;
    }

    /* Open VOS audio device */
    handle = sys_audio_open((uint32_t)desired->freq, bits, desired->channels);
    if (handle < 0) {
        audio_state.error = "SDL_OpenAudio: sys_audio_open failed";
        return -1;
    }

    /* Calculate buffer size */
    /* samples * channels * bytes_per_sample */
    buffer_size = (Uint32)desired->samples * desired->channels * (bits / 8);
    if (buffer_size > AUDIO_BUFFER_MAX) {
        buffer_size = AUDIO_BUFFER_MAX;
    }

    /* Store configuration */
    audio_state.handle = handle;
    audio_state.spec = *desired;
    audio_state.spec.silence = calculate_silence(desired->format);
    audio_state.spec.size = buffer_size;
    audio_state.buffer = static_audio_buffer;
    audio_state.buffer_size = buffer_size;
    audio_state.status = SDL_AUDIO_PAUSED;  /* Start paused per SDL convention */
    audio_state.initialized = 1;
    audio_state.error = NULL;

    /* Fill obtained spec if requested */
    if (obtained) {
        *obtained = audio_state.spec;
    }

    return 0;
}

void SDL_CloseAudio(void)
{
    if (audio_state.handle >= 0) {
        sys_audio_close(audio_state.handle);
        audio_state.handle = -1;
    }

    audio_state.status = SDL_AUDIO_STOPPED;
    audio_state.initialized = 0;
    audio_state.buffer = NULL;
    audio_state.buffer_size = 0;
}

void SDL_PauseAudio(int pause_on)
{
    if (!audio_state.initialized || audio_state.handle < 0) {
        return;
    }

    if (pause_on) {
        audio_state.status = SDL_AUDIO_PAUSED;
    } else {
        audio_state.status = SDL_AUDIO_PLAYING;
    }
}

void SDL_LockAudio(void)
{
    /* No-op: VOS has no threading, so no synchronization needed */
}

void SDL_UnlockAudio(void)
{
    /* No-op: VOS has no threading, so no synchronization needed */
}

SDL_AudioStatus SDL_GetAudioStatus(void)
{
    return audio_state.status;
}

void SDL_PumpAudio(void)
{
    Uint32 chunk_size;
    int written;

    /* Check if audio is ready and playing */
    if (!audio_state.initialized ||
        audio_state.handle < 0 ||
        audio_state.status != SDL_AUDIO_PLAYING ||
        !audio_state.spec.callback) {
        return;
    }

    /*
     * Write audio in small chunks to avoid blocking too long.
     * Since sys_audio_write is blocking, we write smaller pieces
     * to allow the main loop to remain responsive.
     */
    chunk_size = audio_state.buffer_size;

    /* Limit chunk size to avoid long blocking */
    if (chunk_size > 1024) {
        chunk_size = 1024;
    }

    /* Clear buffer with silence before callback fills it */
    memset(audio_state.buffer, audio_state.spec.silence, chunk_size);

    /* Call user callback to get audio data */
    audio_state.spec.callback(
        audio_state.spec.userdata,
        audio_state.buffer,
        (int)chunk_size
    );

    /* Write audio data to device */
    written = sys_audio_write(
        audio_state.handle,
        audio_state.buffer,
        chunk_size
    );

    if (written < 0) {
        audio_state.error = "SDL_PumpAudio: sys_audio_write failed";
    }
}

void SDL_MixAudio(Uint8 *dst, const Uint8 *src, Uint32 len, int volume)
{
    Uint32 i;
    int sample;

    if (!dst || !src || volume == 0) {
        return;
    }

    /* Clamp volume to 0-128 range */
    if (volume < 0) volume = 0;
    if (volume > 128) volume = 128;

    /*
     * Simple mixing for 8-bit unsigned audio.
     * For 16-bit audio, this would need to be modified to handle
     * sample pairs correctly.
     */
    if (audio_state.initialized && SDL_AUDIO_BITSIZE(audio_state.spec.format) == 16) {
        /* 16-bit signed mixing */
        Sint32 *dst16 = (Sint32 *)dst;
        const Sint32 *src16 = (const Sint32 *)src;
        Uint32 samples = len / 2;

        for (i = 0; i < samples; i++) {
            /* Get signed 16-bit samples */
            int16_t dst_sample = ((int16_t *)dst)[i];
            int16_t src_sample = ((const int16_t *)src)[i];

            /* Mix with volume scaling */
            sample = dst_sample + ((src_sample * volume) >> 7);

            /* Clamp to 16-bit signed range */
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;

            ((int16_t *)dst)[i] = (int16_t)sample;
        }
    } else {
        /* 8-bit unsigned mixing (centered at 128) */
        for (i = 0; i < len; i++) {
            /* Convert to signed, mix, convert back */
            int dst_sample = (int)dst[i] - 128;
            int src_sample = (int)src[i] - 128;

            sample = dst_sample + ((src_sample * volume) >> 7);

            /* Clamp to signed 8-bit range then convert to unsigned */
            if (sample > 127) sample = 127;
            if (sample < -128) sample = -128;

            dst[i] = (Uint8)(sample + 128);
        }
    }
}

const char *SDL_GetAudioError(void)
{
    return audio_state.error ? audio_state.error : "";
}
