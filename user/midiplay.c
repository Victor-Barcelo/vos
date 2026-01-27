// midiplay - MIDI file player for VOS
// Uses TinySoundFont for SF2 synthesis and TinyMidiLoader for MIDI parsing

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include "syscall.h"

#define TSF_IMPLEMENTATION
#include "../third_party/tsf/tsf.h"

#define TML_IMPLEMENTATION
#include "../third_party/tsf/tml.h"

#define SAMPLE_RATE 44100
#define BUFFER_SAMPLES 1024

static void print_usage(const char* prog) {
    printf("Usage: %s <file.mid> <soundfont.sf2>\n", prog);
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
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char* midi_file = argv[1];
    const char* sf2_file = argv[2];

    // Load the SoundFont
    printf("Loading SoundFont: %s\n", sf2_file);
    tsf* soundfont = tsf_load_filename(sf2_file);
    if (!soundfont) {
        fprintf(stderr, "Error: Cannot load SoundFont '%s'\n", sf2_file);
        return 1;
    }

    // Set up the synthesizer
    tsf_set_output(soundfont, TSF_STEREO_INTERLEAVED, SAMPLE_RATE, 0.0f);

    // Load the MIDI file
    printf("Loading MIDI: %s\n", midi_file);
    tml_message* midi = tml_load_filename(midi_file);
    if (!midi) {
        fprintf(stderr, "Error: Cannot load MIDI file '%s'\n", midi_file);
        tsf_close(soundfont);
        return 1;
    }

    printf("Press 'q' to quit, SPACE to pause/resume\n\n");

    // Open audio device
    int audio = sys_audio_open(SAMPLE_RATE, 16, 2);
    if (audio < 0) {
        fprintf(stderr, "Error: Cannot open audio device (no Sound Blaster 16?)\n");
        tml_free(midi);
        tsf_close(soundfont);
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

    // Audio buffer
    short pcm_buffer[BUFFER_SAMPLES * 2];  // stereo 16-bit

    int paused = 0;
    int quit = 0;
    double msec = 0.0;
    tml_message* current_msg = midi;

    // Calculate ms per buffer
    double ms_per_buffer = (BUFFER_SAMPLES * 1000.0) / SAMPLE_RATE;

    while (!quit && current_msg) {
        // Check for keyboard input
        if (kbhit()) {
            char c;
            if (read(STDIN_FILENO, &c, 1) == 1) {
                if (c == 'q' || c == 'Q') {
                    quit = 1;
                } else if (c == ' ') {
                    paused = !paused;
                    if (paused) {
                        // Turn off all notes when pausing
                        tsf_note_off_all(soundfont);
                    }
                    printf("%s\n", paused ? "[Paused]" : "[Playing]");
                }
            }
        }

        if (paused) {
            sys_sleep(50);
            continue;
        }

        // Process MIDI messages up to current time
        while (current_msg && current_msg->time <= msec) {
            switch (current_msg->type) {
                case TML_PROGRAM_CHANGE:
                    tsf_channel_set_presetnumber(soundfont, current_msg->channel,
                                                  current_msg->program, (current_msg->channel == 9));
                    break;

                case TML_NOTE_ON:
                    tsf_channel_note_on(soundfont, current_msg->channel,
                                        current_msg->key, current_msg->velocity / 127.0f);
                    break;

                case TML_NOTE_OFF:
                    tsf_channel_note_off(soundfont, current_msg->channel, current_msg->key);
                    break;

                case TML_PITCH_BEND:
                    tsf_channel_set_pitchwheel(soundfont, current_msg->channel,
                                               current_msg->pitch_bend);
                    break;

                case TML_CONTROL_CHANGE:
                    tsf_channel_midi_control(soundfont, current_msg->channel,
                                             current_msg->control, current_msg->control_value);
                    break;

                default:
                    break;
            }
            current_msg = current_msg->next;
        }

        // Render audio
        tsf_render_short(soundfont, pcm_buffer, BUFFER_SAMPLES, 0);

        // Show progress
        static int last_sec = -1;
        int cur_sec = (int)(msec / 1000.0);
        if (cur_sec != last_sec) {
            last_sec = cur_sec;
            printf("\rTime: %d:%02d  ", cur_sec / 60, cur_sec % 60);
            fflush(stdout);
        }

        // Write to audio device
        int bytes_to_write = BUFFER_SAMPLES * 2 * sizeof(short);
        int written = sys_audio_write(audio, pcm_buffer, bytes_to_write);
        if (written < 0) {
            fprintf(stderr, "\nError: Audio write failed\n");
            break;
        }

        // Advance time
        msec += ms_per_buffer;
    }

    printf("\n");
    if (!current_msg && !quit) {
        printf("[Song complete]\n");
    }

    // Restore terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

    // Clean up
    sys_audio_close(audio);
    tml_free(midi);
    tsf_close(soundfont);

    return 0;
}
