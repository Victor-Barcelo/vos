// Example: stb_hexwave.h - Procedural Audio Synthesis
// Compile: tcc ex_stb_hexwave.c -lm -o ex_stb_hexwave

#include <stdio.h>
#include <math.h>

#define STB_HEXWAVE_IMPLEMENTATION
#include "../stb_hexwave.h"

// Simple WAV file writer for testing
void write_wav_header(FILE* f, int sample_rate, int num_samples) {
    int byte_rate = sample_rate * 2;  // 16-bit mono
    int data_size = num_samples * 2;
    int file_size = 36 + data_size;

    // RIFF header
    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    int fmt_size = 16;
    short audio_format = 1;  // PCM
    short num_channels = 1;
    short bits_per_sample = 16;
    short block_align = 2;
    fwrite(&fmt_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);

    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
}

int main(void) {
    printf("=== stb_hexwave.h (Audio Synthesis) Example ===\n\n");

    printf("stb_hexwave generates audio waveforms for sound effects.\n");
    printf("It can create various tones, sweeps, and retro game sounds.\n\n");

    // Initialize hexwave
    stb_hexwave_init();
    printf("Hexwave initialized.\n\n");

    int sample_rate = 44100;
    int duration_ms = 500;
    int num_samples = sample_rate * duration_ms / 1000;

    // Allocate sample buffer
    float* samples = (float*)malloc(num_samples * sizeof(float));
    short* samples_16bit = (short*)malloc(num_samples * sizeof(short));

    // ==========================================
    // Example 1: Simple sine-like tone
    // ==========================================
    printf("--- Sound 1: Simple Tone (440Hz) ---\n");

    STB_HEXWAVE_s tone;
    stb_hexwave_create(&tone, 10, 0.5f, 0, 0, 0, 0);  // Simple waveform

    float t = 0;
    float freq = 440.0f;
    float dt = 1.0f / sample_rate;

    for (int i = 0; i < num_samples; i++) {
        // Generate sample
        samples[i] = stb_hexwave_sample(&tone, t, freq);
        t += dt;

        // Apply simple envelope (fade out)
        float envelope = 1.0f - (float)i / num_samples;
        samples[i] *= envelope * 0.5f;
    }

    printf("Generated %d samples at %dHz\n", num_samples, sample_rate);

    // Show waveform ASCII visualization
    printf("Waveform preview (first 50 samples):\n");
    for (int y = 4; y >= -4; y--) {
        printf("%+2d |", y);
        for (int x = 0; x < 50; x++) {
            int sample_idx = x * 10;
            if (sample_idx < num_samples) {
                int level = (int)(samples[sample_idx] * 4);
                if (level == y) printf("*");
                else if (y == 0) printf("-");
                else printf(" ");
            }
        }
        printf("\n");
    }

    // ==========================================
    // Example 2: Frequency sweep (laser sound)
    // ==========================================
    printf("\n--- Sound 2: Frequency Sweep (Laser) ---\n");

    float start_freq = 1000.0f;
    float end_freq = 200.0f;
    t = 0;

    for (int i = 0; i < num_samples; i++) {
        float progress = (float)i / num_samples;
        float current_freq = start_freq + (end_freq - start_freq) * progress;
        samples[i] = stb_hexwave_sample(&tone, t, current_freq);
        t += dt;

        // Envelope
        float envelope = 1.0f - progress;
        samples[i] *= envelope * 0.5f;
    }

    printf("Generated sweep from %.0fHz to %.0fHz\n", start_freq, end_freq);

    // ==========================================
    // Example 3: Different waveform shapes
    // ==========================================
    printf("\n--- Sound 3: Different Waveforms ---\n");

    // Square-ish wave
    STB_HEXWAVE_s square_wave;
    stb_hexwave_create(&square_wave, 2, 0.8f, 0, 0, 0.5f, 0.5f);
    printf("Square wave created.\n");

    // Saw-ish wave
    STB_HEXWAVE_s saw_wave;
    stb_hexwave_create(&saw_wave, 6, 0.9f, 0.1f, 0.9f, 0, 1);
    printf("Saw wave created.\n");

    // ==========================================
    // Save one sound to WAV file
    // ==========================================
    printf("\n--- Saving to WAV file ---\n");

    // Convert to 16-bit
    for (int i = 0; i < num_samples; i++) {
        int val = (int)(samples[i] * 32767);
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        samples_16bit[i] = (short)val;
    }

    FILE* wav = fopen("hexwave_test.wav", "wb");
    if (wav) {
        write_wav_header(wav, sample_rate, num_samples);
        fwrite(samples_16bit, 2, num_samples, wav);
        fclose(wav);
        printf("Saved 'hexwave_test.wav' (%d ms, %d samples)\n", duration_ms, num_samples);
    } else {
        printf("Could not create WAV file.\n");
    }

    // Cleanup
    free(samples);
    free(samples_16bit);

    printf("\nDone!\n");
    return 0;
}
