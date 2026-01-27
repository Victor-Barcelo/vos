// modplay - MOD file player for VOS
// Uses pocketmod library for MOD playback

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include "syscall.h"

#define POCKETMOD_IMPLEMENTATION
#include "../third_party/pocketmod/pocketmod.h"

#define SAMPLE_RATE 22050
#define BUFFER_SAMPLES 2048
#define BUFFER_SIZE (BUFFER_SAMPLES * 2 * sizeof(int16_t))  // stereo 16-bit

// Convert float samples to signed 16-bit
static void float_to_s16(const float* src, int16_t* dst, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        float s = src[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        dst[i] = (int16_t)(s * 32767.0f);
    }
}

static void print_usage(const char* prog) {
    printf("Usage: %s <file.mod>\n", prog);
    printf("Controls:\n");
    printf("  SPACE - Pause/Resume\n");
    printf("  q     - Quit\n");
}

static int kbhit(void) {
    vos_pollfd_t pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = VOS_POLLIN;
    pfd.revents = 0;
    int ret = sys_poll(&pfd, 1, 0);
    return (ret > 0 && (pfd.revents & VOS_POLLIN));
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* filename = argv[1];

    // Open and read the MOD file
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open '%s'\n", filename);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 2 * 1024 * 1024) {
        fprintf(stderr, "Error: Invalid file size\n");
        fclose(f);
        return 1;
    }

    uint8_t* mod_data = malloc(file_size);
    if (!mod_data) {
        fprintf(stderr, "Error: Out of memory\n");
        fclose(f);
        return 1;
    }

    if (fread(mod_data, 1, file_size, f) != (size_t)file_size) {
        fprintf(stderr, "Error: Failed to read file\n");
        free(mod_data);
        fclose(f);
        return 1;
    }
    fclose(f);

    // Initialize pocketmod
    pocketmod_context ctx;
    if (!pocketmod_init(&ctx, mod_data, (int)file_size, SAMPLE_RATE)) {
        fprintf(stderr, "Error: Not a valid MOD file\n");
        free(mod_data);
        return 1;
    }

    printf("Playing: %s\n", filename);
    printf("Channels: %d, Patterns: %d\n", ctx.num_channels, ctx.num_patterns);
    printf("Press 'q' to quit, SPACE to pause/resume\n\n");

    // Open audio device
    int audio = sys_audio_open(SAMPLE_RATE, 16, 2);
    if (audio < 0) {
        fprintf(stderr, "Error: Cannot open audio device (no Sound Blaster 16?)\n");
        free(mod_data);
        return 1;
    }

    // Set terminal to non-blocking for keyboard input
    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 0;
    new_term.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    // Audio buffers
    float float_buffer[BUFFER_SAMPLES * 2];  // stereo floats
    int16_t pcm_buffer[BUFFER_SAMPLES * 2];  // stereo 16-bit

    int paused = 0;
    int quit = 0;
    int last_pattern = -1;

    while (!quit) {
        // Check for keyboard input
        if (kbhit()) {
            char c;
            if (read(STDIN_FILENO, &c, 1) == 1) {
                if (c == 'q' || c == 'Q') {
                    quit = 1;
                } else if (c == ' ') {
                    paused = !paused;
                    printf("%s\n", paused ? "[Paused]" : "[Playing]");
                }
            }
        }

        if (paused) {
            sys_sleep(50);
            continue;
        }

        // Render audio
        int rendered = pocketmod_render(&ctx, float_buffer, sizeof(float_buffer));
        if (rendered == 0) {
            // Check if we've looped
            if (pocketmod_loop_count(&ctx) > 0) {
                printf("\n[Song complete - looped]\n");
                break;
            }
        }

        // Show current pattern
        if (ctx.pattern != last_pattern) {
            last_pattern = ctx.pattern;
            printf("\rPattern: %d/%d  ", ctx.pattern + 1, ctx.length);
            fflush(stdout);
        }

        // Convert float to 16-bit signed
        int num_samples = rendered / sizeof(float);
        float_to_s16(float_buffer, pcm_buffer, num_samples);

        // Write to audio device
        int bytes_to_write = num_samples * sizeof(int16_t);
        int written = sys_audio_write(audio, pcm_buffer, bytes_to_write);
        if (written < 0) {
            fprintf(stderr, "\nError: Audio write failed\n");
            break;
        }
    }

    printf("\n");

    // Restore terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

    // Clean up
    sys_audio_close(audio);
    free(mod_data);

    return 0;
}
