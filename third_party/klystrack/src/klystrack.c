/*
 * klystrack - Simple Chiptune Tracker for VOS
 * A lightweight music tracker with chip sound synthesis
 *
 * Controls:
 *   Arrow keys  - Navigate pattern
 *   0-9, A-G    - Enter notes (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
 *   +/-         - Octave up/down
 *   Space       - Play/Stop
 *   Tab         - Switch channel
 *   Page Up/Dn  - Pattern up/down
 *   Delete      - Clear note
 *   Escape/Q    - Quit
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Screen dimensions */
#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480

/* Tracker configuration */
#define NUM_CHANNELS    4
#define PATTERN_LENGTH  64
#define NUM_PATTERNS    16
#define NUM_INSTRUMENTS 8

/* Audio configuration */
#define SAMPLE_RATE     44100
#define AUDIO_BUFFER    1024

/* Note values (0 = empty, 1-96 = C-0 to B-7) */
#define NOTE_EMPTY      0
#define NOTE_OFF        255

/* Waveform types */
#define WAVE_SQUARE     0
#define WAVE_SAW        1
#define WAVE_TRIANGLE   2
#define WAVE_NOISE      3

/* Colors */
#define COL_BG          0x1a1a2e
#define COL_HEADER      0x16213e
#define COL_ROW_DARK    0x0f0f23
#define COL_ROW_LIGHT   0x1a1a2e
#define COL_ROW_BEAT    0x2a2a3e
#define COL_CURSOR      0x4a69bd
#define COL_CURSOR_BG   0x2d3a5a
#define COL_TEXT        0xe0e0e0
#define COL_TEXT_DIM    0x808080
#define COL_NOTE_C1     0x7ed6df
#define COL_NOTE_C2     0xf8a5c2
#define COL_NOTE_C3     0xf5cd79
#define COL_NOTE_C4     0x78e08f
#define COL_PLAYING     0x00ff00
#define COL_CHANNEL     0xffc107

/* Pattern note structure */
typedef struct {
    uint8_t note;       /* Note value (0=empty, 1-96=note, 255=off) */
    uint8_t instrument; /* Instrument number (0-7) */
    uint8_t volume;     /* Volume (0-64, 255=use instrument default) */
    uint8_t effect;     /* Effect type */
    uint8_t effect_val; /* Effect value */
} pattern_note_t;

/* Instrument definition */
typedef struct {
    uint8_t waveform;   /* Waveform type */
    uint8_t attack;     /* Attack time (0-255) */
    uint8_t decay;      /* Decay time (0-255) */
    uint8_t sustain;    /* Sustain level (0-255) */
    uint8_t release;    /* Release time (0-255) */
    uint8_t duty;       /* Duty cycle for square wave (0-255) */
    char name[16];      /* Instrument name */
} instrument_t;

/* Channel state for audio */
typedef struct {
    double phase;       /* Current phase (0-1) */
    double freq;        /* Frequency in Hz */
    double volume;      /* Current volume (0-1) */
    double env_level;   /* Envelope level (0-1) */
    int env_stage;      /* ADSR stage: 0=attack, 1=decay, 2=sustain, 3=release */
    int note_on;        /* Is note currently on? */
    int instrument;     /* Current instrument */
    uint32_t lfsr;      /* LFSR for noise generation */
} channel_state_t;

/* Global state */
static pattern_note_t patterns[NUM_PATTERNS][NUM_CHANNELS][PATTERN_LENGTH];
static instrument_t instruments[NUM_INSTRUMENTS];
static channel_state_t channels[NUM_CHANNELS];

static int cursor_row = 0;
static int cursor_channel = 0;
static int cursor_column = 0;  /* 0=note, 1=instrument, 2=volume, 3=effect */
static int current_pattern = 0;
static int current_octave = 4;

static int is_playing = 0;
static int play_row = 0;
static double play_tick = 0;
static int tempo = 125;  /* BPM */
static int speed = 6;    /* Ticks per row */

/* Note frequency table (A4 = 440Hz) */
static const double note_freqs[12] = {
    261.63, 277.18, 293.66, 311.13, 329.63, 349.23,  /* C4 to F4 */
    369.99, 392.00, 415.30, 440.00, 466.16, 493.88   /* F#4 to B4 */
};

/* Note names */
static const char* note_names[12] = {
    "C-", "C#", "D-", "D#", "E-", "F-",
    "F#", "G-", "G#", "A-", "A#", "B-"
};

/* Get frequency for a note value (1-96) */
static double get_note_freq(int note) {
    if (note == NOTE_EMPTY || note == NOTE_OFF) return 0;
    int octave = (note - 1) / 12;
    int semitone = (note - 1) % 12;
    double base_freq = note_freqs[semitone];
    /* Adjust for octave (relative to octave 4) */
    int octave_diff = octave - 4;
    if (octave_diff > 0) {
        for (int i = 0; i < octave_diff; i++) base_freq *= 2.0;
    } else if (octave_diff < 0) {
        for (int i = 0; i < -octave_diff; i++) base_freq /= 2.0;
    }
    return base_freq;
}

/* Get note name string */
static void get_note_string(int note, char* buf) {
    if (note == NOTE_EMPTY) {
        strcpy(buf, "...");
        return;
    }
    if (note == NOTE_OFF) {
        strcpy(buf, "===");
        return;
    }
    int octave = (note - 1) / 12;
    int semitone = (note - 1) % 12;
    sprintf(buf, "%s%d", note_names[semitone], octave);
}

/* Initialize default instruments */
static void init_instruments(void) {
    /* Square lead */
    instruments[0] = (instrument_t){
        .waveform = WAVE_SQUARE,
        .attack = 5, .decay = 20, .sustain = 180, .release = 30,
        .duty = 128, .name = "Square Lead"
    };
    /* Saw bass */
    instruments[1] = (instrument_t){
        .waveform = WAVE_SAW,
        .attack = 2, .decay = 30, .sustain = 160, .release = 20,
        .duty = 0, .name = "Saw Bass"
    };
    /* Triangle */
    instruments[2] = (instrument_t){
        .waveform = WAVE_TRIANGLE,
        .attack = 10, .decay = 10, .sustain = 200, .release = 50,
        .duty = 0, .name = "Soft Tri"
    };
    /* Noise */
    instruments[3] = (instrument_t){
        .waveform = WAVE_NOISE,
        .attack = 1, .decay = 40, .sustain = 0, .release = 10,
        .duty = 0, .name = "Noise Hit"
    };
    /* PWM */
    instruments[4] = (instrument_t){
        .waveform = WAVE_SQUARE,
        .attack = 20, .decay = 50, .sustain = 150, .release = 80,
        .duty = 64, .name = "Thin Pulse"
    };
    /* Pluck */
    instruments[5] = (instrument_t){
        .waveform = WAVE_SAW,
        .attack = 1, .decay = 80, .sustain = 0, .release = 5,
        .duty = 0, .name = "Pluck"
    };
    /* Pad */
    instruments[6] = (instrument_t){
        .waveform = WAVE_TRIANGLE,
        .attack = 100, .decay = 20, .sustain = 220, .release = 150,
        .duty = 0, .name = "Soft Pad"
    };
    /* Kick-like */
    instruments[7] = (instrument_t){
        .waveform = WAVE_TRIANGLE,
        .attack = 1, .decay = 15, .sustain = 0, .release = 5,
        .duty = 0, .name = "Kick"
    };
}

/* Initialize a demo pattern */
static void init_demo_pattern(void) {
    /* Clear all patterns */
    memset(patterns, 0, sizeof(patterns));

    /* Simple bass line in channel 0 */
    int bass_notes[] = {25, 0, 0, 0, 25, 0, 0, 0, 28, 0, 0, 0, 28, 0, 0, 0};  /* C2, D#2 */
    for (int i = 0; i < 16; i++) {
        if (bass_notes[i] != 0) {
            patterns[0][0][i * 4].note = bass_notes[i];
            patterns[0][0][i * 4].instrument = 1;
            patterns[0][0][i * 4].volume = 48;
        }
    }

    /* Lead melody in channel 1 */
    int lead_notes[] = {49, 0, 52, 0, 54, 0, 52, 0, 49, 0, 0, 0, 47, 0, 0, 0};  /* C4, D#4, F4 */
    for (int i = 0; i < 16; i++) {
        if (lead_notes[i] != 0) {
            patterns[0][1][i * 4].note = lead_notes[i];
            patterns[0][1][i * 4].instrument = 0;
            patterns[0][1][i * 4].volume = 40;
        }
    }

    /* Arpeggio in channel 2 */
    int arp[] = {37, 40, 44, 40};  /* C3, D#3, G3 */
    for (int i = 0; i < 64; i++) {
        if (i % 4 == 0) {
            patterns[0][2][i].note = arp[(i / 4) % 4];
            patterns[0][2][i].instrument = 2;
            patterns[0][2][i].volume = 32;
        }
    }

    /* Noise rhythm in channel 3 */
    for (int i = 0; i < 64; i += 4) {
        patterns[0][3][i].note = 49;
        patterns[0][3][i].instrument = 3;
        patterns[0][3][i].volume = (i % 16 == 0) ? 50 : 30;
    }
}

/* Generate waveform sample */
static double generate_sample(channel_state_t* ch, instrument_t* inst) {
    if (!ch->note_on && ch->env_stage != 3) return 0;
    if (ch->freq == 0) return 0;

    double sample = 0;
    double duty = (double)inst->duty / 255.0;
    if (duty < 0.1) duty = 0.5;  /* Default to 50% duty */

    switch (inst->waveform) {
        case WAVE_SQUARE:
            sample = (ch->phase < duty) ? 1.0 : -1.0;
            break;
        case WAVE_SAW:
            sample = 2.0 * ch->phase - 1.0;
            break;
        case WAVE_TRIANGLE:
            if (ch->phase < 0.5) {
                sample = 4.0 * ch->phase - 1.0;
            } else {
                sample = 3.0 - 4.0 * ch->phase;
            }
            break;
        case WAVE_NOISE:
            /* Simple LFSR noise */
            ch->lfsr ^= ch->lfsr >> 7;
            ch->lfsr ^= ch->lfsr << 9;
            ch->lfsr ^= ch->lfsr >> 13;
            sample = ((ch->lfsr & 1) ? 1.0 : -1.0);
            break;
    }

    /* Apply envelope */
    sample *= ch->env_level * ch->volume;

    /* Advance phase */
    ch->phase += ch->freq / SAMPLE_RATE;
    while (ch->phase >= 1.0) ch->phase -= 1.0;

    return sample;
}

/* Update envelope for a channel */
static void update_envelope(channel_state_t* ch, instrument_t* inst) {
    double attack_rate = 1.0 / (1 + inst->attack * 100);
    double decay_rate = 1.0 / (1 + inst->decay * 200);
    double sustain_level = inst->sustain / 255.0;
    double release_rate = 1.0 / (1 + inst->release * 300);

    switch (ch->env_stage) {
        case 0: /* Attack */
            ch->env_level += attack_rate;
            if (ch->env_level >= 1.0) {
                ch->env_level = 1.0;
                ch->env_stage = 1;
            }
            break;
        case 1: /* Decay */
            ch->env_level -= decay_rate;
            if (ch->env_level <= sustain_level) {
                ch->env_level = sustain_level;
                ch->env_stage = 2;
            }
            break;
        case 2: /* Sustain */
            ch->env_level = sustain_level;
            break;
        case 3: /* Release */
            ch->env_level -= release_rate;
            if (ch->env_level <= 0) {
                ch->env_level = 0;
                ch->note_on = 0;
            }
            break;
    }
}

/* Trigger note on a channel */
static void trigger_note(int ch_num, int note, int inst_num, int vol) {
    channel_state_t* ch = &channels[ch_num];

    if (note == NOTE_OFF) {
        ch->env_stage = 3;  /* Start release */
        return;
    }

    if (note == NOTE_EMPTY) return;

    ch->freq = get_note_freq(note);
    ch->instrument = inst_num;
    ch->volume = (vol == 255) ? 0.5 : (vol / 64.0) * 0.5;
    ch->phase = 0;
    ch->env_level = 0;
    ch->env_stage = 0;  /* Start attack */
    ch->note_on = 1;
    ch->lfsr = 0x1234;  /* Reset noise */
}

/* Audio callback (called from SDL_PumpAudio) */
static void audio_callback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    int16_t* out = (int16_t*)stream;
    int samples = len / sizeof(int16_t) / 2;  /* Stereo */

    for (int i = 0; i < samples; i++) {
        double left = 0, right = 0;

        for (int c = 0; c < NUM_CHANNELS; c++) {
            update_envelope(&channels[c], &instruments[channels[c].instrument]);
            double sample = generate_sample(&channels[c], &instruments[channels[c].instrument]);
            /* Simple panning: channels 0,2 left, 1,3 right */
            if (c == 0 || c == 2) {
                left += sample * 0.7;
                right += sample * 0.3;
            } else {
                left += sample * 0.3;
                right += sample * 0.7;
            }
        }

        /* Clamp and convert to 16-bit */
        if (left > 1.0) left = 1.0;
        if (left < -1.0) left = -1.0;
        if (right > 1.0) right = 1.0;
        if (right < -1.0) right = -1.0;

        out[i * 2] = (int16_t)(left * 30000);
        out[i * 2 + 1] = (int16_t)(right * 30000);
    }
}

/* Process a tracker row */
static void process_row(int pattern, int row) {
    for (int c = 0; c < NUM_CHANNELS; c++) {
        pattern_note_t* note = &patterns[pattern][c][row];
        if (note->note != NOTE_EMPTY) {
            trigger_note(c, note->note, note->instrument, note->volume);
        }
    }
}

/* Draw filled rectangle */
static void draw_rect(SDL_Renderer* r, int x, int y, int w, int h, uint32_t color) {
    SDL_SetRenderDrawColor(r, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

/* Simple font rendering (8x8 characters) */
static void draw_char(SDL_Renderer* r, int x, int y, char c, uint32_t color) {
    /* Very simple font rendering - just draw colored blocks for now */
    SDL_SetRenderDrawColor(r, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255);

    /* Draw a simple representation */
    if (c == ' ' || c == '.') return;

    /* Simple block for characters */
    SDL_Rect rect = {x + 1, y + 1, 6, 6};
    SDL_RenderFillRect(r, &rect);
}

/* Draw text string */
static void draw_text(SDL_Renderer* r, int x, int y, const char* text, uint32_t color) {
    SDL_SetRenderDrawColor(r, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255);

    /* Since we don't have real font rendering, draw simple colored bars to indicate text */
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        if (text[i] != ' ' && text[i] != '.') {
            SDL_Rect rect = {x + i * 8 + 1, y + 2, 6, 10};
            SDL_RenderFillRect(r, &rect);
        }
    }
}

/* Simplified text rendering - draw characters as patterns */
static void draw_char_pattern(SDL_Renderer* r, int x, int y, char c, uint32_t color) {
    SDL_SetRenderDrawColor(r, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255);

    /* Draw simple 5x7 patterns for digits and letters */
    /* This is a very simplified approach */
    int px, py;

    switch(c) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            /* Draw digit-like pattern */
            SDL_RenderDrawLine(r, x+1, y, x+5, y);
            SDL_RenderDrawLine(r, x+1, y+6, x+5, y+6);
            SDL_RenderDrawLine(r, x, y+1, x, y+5);
            SDL_RenderDrawLine(r, x+6, y+1, x+6, y+5);
            if (c == '0' || c == '2' || c == '3' || c == '5' || c == '6' || c == '8' || c == '9')
                SDL_RenderDrawLine(r, x+1, y+3, x+5, y+3);
            break;
        case 'C': case 'D': case 'E': case 'F': case 'G': case 'A': case 'B':
            /* Draw letter-like pattern */
            SDL_RenderDrawLine(r, x, y, x, y+7);
            SDL_RenderDrawLine(r, x+1, y, x+6, y);
            SDL_RenderDrawLine(r, x+1, y+7, x+6, y+7);
            if (c != 'C') SDL_RenderDrawLine(r, x+1, y+3, x+5, y+3);
            break;
        case '#':
            /* Sharp symbol */
            SDL_RenderDrawLine(r, x+2, y, x+2, y+7);
            SDL_RenderDrawLine(r, x+5, y, x+5, y+7);
            SDL_RenderDrawLine(r, x, y+2, x+7, y+2);
            SDL_RenderDrawLine(r, x, y+5, x+7, y+5);
            break;
        case '-':
            SDL_RenderDrawLine(r, x+1, y+3, x+5, y+3);
            break;
        case '=':
            SDL_RenderDrawLine(r, x+1, y+2, x+5, y+2);
            SDL_RenderDrawLine(r, x+1, y+4, x+5, y+4);
            SDL_RenderDrawLine(r, x+1, y+6, x+5, y+6);
            break;
        case '.':
            px = x + 3; py = y + 6;
            SDL_RenderDrawPoint(r, px, py);
            SDL_RenderDrawPoint(r, px+1, py);
            SDL_RenderDrawPoint(r, px, py+1);
            SDL_RenderDrawPoint(r, px+1, py+1);
            break;
        default:
            /* Generic character - draw a small block */
            if (c != ' ') {
                SDL_Rect rect = {x+1, y+1, 5, 6};
                SDL_RenderFillRect(r, &rect);
            }
            break;
    }
}

/* Draw a string using character patterns */
static void draw_string(SDL_Renderer* r, int x, int y, const char* str, uint32_t color) {
    int offset = 0;
    while (*str) {
        draw_char_pattern(r, x + offset, y, *str, color);
        offset += 8;
        str++;
    }
}

/* Draw the tracker UI */
static void draw_ui(SDL_Renderer* r) {
    /* Clear background */
    draw_rect(r, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COL_BG);

    /* Header */
    draw_rect(r, 0, 0, SCREEN_WIDTH, 40, COL_HEADER);
    draw_string(r, 10, 10, "KLYSTRACK", COL_TEXT);

    /* Status info */
    char status[64];
    sprintf(status, "PAT %02d  OCT %d  BPM %d", current_pattern, current_octave, tempo);
    draw_string(r, 200, 10, status, COL_TEXT);

    if (is_playing) {
        draw_string(r, 500, 10, "PLAY", COL_PLAYING);
    } else {
        draw_string(r, 500, 10, "STOP", COL_TEXT_DIM);
    }

    /* Channel headers */
    int ch_width = 140;
    int ch_start = 50;
    for (int c = 0; c < NUM_CHANNELS; c++) {
        int x = ch_start + c * ch_width;
        uint32_t col = (c == cursor_channel) ? COL_CHANNEL : COL_TEXT_DIM;
        char ch_name[16];
        sprintf(ch_name, "CH %d", c + 1);
        draw_string(r, x + 40, 50, ch_name, col);
    }

    /* Pattern display */
    int row_height = 12;
    int start_row = cursor_row - 15;
    if (start_row < 0) start_row = 0;

    for (int row = start_row; row < start_row + 32 && row < PATTERN_LENGTH; row++) {
        int y = 70 + (row - start_row) * row_height;

        /* Row background */
        uint32_t row_bg = COL_ROW_DARK;
        if (row % 4 == 0) row_bg = COL_ROW_BEAT;
        else if (row % 2 == 0) row_bg = COL_ROW_LIGHT;

        if (row == cursor_row) {
            row_bg = COL_CURSOR_BG;
        }
        if (is_playing && row == play_row) {
            row_bg = COL_PLAYING & 0x3f3f3f;  /* Dim green */
        }

        draw_rect(r, 0, y, SCREEN_WIDTH, row_height, row_bg);

        /* Row number */
        char row_str[8];
        sprintf(row_str, "%02X", row);
        draw_string(r, 10, y + 2, row_str, COL_TEXT_DIM);

        /* Channel data */
        for (int c = 0; c < NUM_CHANNELS; c++) {
            int x = ch_start + c * ch_width;
            pattern_note_t* note = &patterns[current_pattern][c][row];

            /* Note */
            char note_str[8];
            get_note_string(note->note, note_str);

            uint32_t note_col = COL_TEXT;
            if (note->note == NOTE_EMPTY) {
                note_col = COL_TEXT_DIM;
            } else if (note->note != NOTE_OFF) {
                int oct = (note->note - 1) / 12;
                switch (oct % 4) {
                    case 0: note_col = COL_NOTE_C1; break;
                    case 1: note_col = COL_NOTE_C2; break;
                    case 2: note_col = COL_NOTE_C3; break;
                    case 3: note_col = COL_NOTE_C4; break;
                }
            }

            /* Highlight cursor position */
            if (row == cursor_row && c == cursor_channel) {
                draw_rect(r, x - 2, y, ch_width - 4, row_height, COL_CURSOR);
            }

            draw_string(r, x, y + 2, note_str, note_col);

            /* Instrument */
            if (note->note != NOTE_EMPTY && note->note != NOTE_OFF) {
                char inst_str[4];
                sprintf(inst_str, "%02X", note->instrument);
                draw_string(r, x + 30, y + 2, inst_str, COL_TEXT_DIM);

                /* Volume */
                if (note->volume != 255 && note->volume != 0) {
                    char vol_str[4];
                    sprintf(vol_str, "%02X", note->volume);
                    draw_string(r, x + 55, y + 2, vol_str, COL_TEXT_DIM);
                }
            }
        }
    }

    /* Help text at bottom */
    draw_rect(r, 0, SCREEN_HEIGHT - 30, SCREEN_WIDTH, 30, COL_HEADER);
    draw_string(r, 10, SCREEN_HEIGHT - 22,
                "SPACE:PLAY  Q:QUIT  ARROWS:NAV  TAB:CHAN  +/-:OCT  0-G:NOTE  DEL:CLEAR",
                COL_TEXT_DIM);
}

/* Handle keyboard input */
static int handle_input(SDL_Event* event) {
    if (event->type == SDL_QUIT) {
        return 0;
    }

    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;

        switch (key) {
            case SDLK_ESCAPE:
            case SDLK_q:
                return 0;  /* Quit */

            case SDLK_SPACE:
                is_playing = !is_playing;
                if (is_playing) {
                    play_row = cursor_row;
                    play_tick = 0;
                }
                break;

            case SDLK_UP:
                if (cursor_row > 0) cursor_row--;
                break;
            case SDLK_DOWN:
                if (cursor_row < PATTERN_LENGTH - 1) cursor_row++;
                break;
            case SDLK_LEFT:
                if (cursor_channel > 0) cursor_channel--;
                break;
            case SDLK_RIGHT:
                if (cursor_channel < NUM_CHANNELS - 1) cursor_channel++;
                break;

            case SDLK_TAB:
                cursor_channel = (cursor_channel + 1) % NUM_CHANNELS;
                break;

            case SDLK_PAGEUP:
                if (current_pattern > 0) current_pattern--;
                break;
            case SDLK_PAGEDOWN:
                if (current_pattern < NUM_PATTERNS - 1) current_pattern++;
                break;

            case SDLK_PLUS:
            case SDLK_EQUALS:
            case SDLK_KP_PLUS:
                if (current_octave < 7) current_octave++;
                break;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
                if (current_octave > 0) current_octave--;
                break;

            case SDLK_DELETE:
            case SDLK_BACKSPACE:
                patterns[current_pattern][cursor_channel][cursor_row].note = NOTE_EMPTY;
                patterns[current_pattern][cursor_channel][cursor_row].instrument = 0;
                patterns[current_pattern][cursor_channel][cursor_row].volume = 255;
                break;

            case SDLK_RETURN:
                /* Note off */
                patterns[current_pattern][cursor_channel][cursor_row].note = NOTE_OFF;
                if (cursor_row < PATTERN_LENGTH - 1) cursor_row++;
                break;

            /* Note input - piano keyboard layout */
            case SDLK_z: /* C */
            case SDLK_x: /* D */
            case SDLK_c: /* E */
            case SDLK_v: /* F */
            case SDLK_b: /* G */
            case SDLK_n: /* A */
            case SDLK_m: /* B */
            case SDLK_s: /* C# */
            case SDLK_d: /* D# */
            case SDLK_g: /* F# */
            case SDLK_h: /* G# */
            case SDLK_j: /* A# */
                {
                    int semitone = -1;
                    switch (key) {
                        case SDLK_z: semitone = 0; break;  /* C */
                        case SDLK_s: semitone = 1; break;  /* C# */
                        case SDLK_x: semitone = 2; break;  /* D */
                        case SDLK_d: semitone = 3; break;  /* D# */
                        case SDLK_c: semitone = 4; break;  /* E */
                        case SDLK_v: semitone = 5; break;  /* F */
                        case SDLK_g: semitone = 6; break;  /* F# */
                        case SDLK_b: semitone = 7; break;  /* G */
                        case SDLK_h: semitone = 8; break;  /* G# */
                        case SDLK_n: semitone = 9; break;  /* A */
                        case SDLK_j: semitone = 10; break; /* A# */
                        case SDLK_m: semitone = 11; break; /* B */
                    }
                    if (semitone >= 0) {
                        int note = current_octave * 12 + semitone + 1;
                        if (note > 0 && note <= 96) {
                            patterns[current_pattern][cursor_channel][cursor_row].note = note;
                            patterns[current_pattern][cursor_channel][cursor_row].instrument = cursor_channel % NUM_INSTRUMENTS;
                            patterns[current_pattern][cursor_channel][cursor_row].volume = 48;

                            /* Preview note */
                            trigger_note(cursor_channel, note, cursor_channel % NUM_INSTRUMENTS, 48);

                            if (cursor_row < PATTERN_LENGTH - 1) cursor_row++;
                        }
                    }
                }
                break;

            /* Upper octave notes */
            case SDLK_w: /* C+1 */
            case SDLK_e: /* D+1 */
            case SDLK_r: /* E+1 */
            case SDLK_t: /* F+1 */
            case SDLK_y: /* G+1 */
            case SDLK_u: /* A+1 */
            case SDLK_i: /* B+1 */
            case SDLK_3: /* C#+1 */
            case SDLK_4: /* D#+1 */
            case SDLK_6: /* F#+1 */
            case SDLK_7: /* G#+1 */
            case SDLK_8: /* A#+1 */
                {
                    int semitone = -1;
                    switch (key) {
                        case SDLK_w: semitone = 0; break;  /* C */
                        case SDLK_3: semitone = 1; break;  /* C# */
                        case SDLK_e: semitone = 2; break;  /* D */
                        case SDLK_4: semitone = 3; break;  /* D# */
                        case SDLK_r: semitone = 4; break;  /* E */
                        case SDLK_t: semitone = 5; break;  /* F */
                        case SDLK_6: semitone = 6; break;  /* F# */
                        case SDLK_y: semitone = 7; break;  /* G */
                        case SDLK_7: semitone = 8; break;  /* G# */
                        case SDLK_u: semitone = 9; break;  /* A */
                        case SDLK_8: semitone = 10; break; /* A# */
                        case SDLK_i: semitone = 11; break; /* B */
                    }
                    if (semitone >= 0) {
                        int note = (current_octave + 1) * 12 + semitone + 1;
                        if (note > 0 && note <= 96) {
                            patterns[current_pattern][cursor_channel][cursor_row].note = note;
                            patterns[current_pattern][cursor_channel][cursor_row].instrument = cursor_channel % NUM_INSTRUMENTS;
                            patterns[current_pattern][cursor_channel][cursor_row].volume = 48;

                            /* Preview note */
                            trigger_note(cursor_channel, note, cursor_channel % NUM_INSTRUMENTS, 48);

                            if (cursor_row < PATTERN_LENGTH - 1) cursor_row++;
                        }
                    }
                }
                break;

            /* Instrument selection with number keys when shift is held */
            case SDLK_1:
            case SDLK_2:
                {
                    /* Change instrument for current position */
                    int inst = key - SDLK_1;
                    if (patterns[current_pattern][cursor_channel][cursor_row].note != NOTE_EMPTY) {
                        patterns[current_pattern][cursor_channel][cursor_row].instrument = inst;
                    }
                }
                break;
        }
    }

    return 1;  /* Continue running */
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("Klystrack - Chiptune Tracker for VOS\n");
    printf("Initializing...\n");

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Create window */
    SDL_Window* window = SDL_CreateWindow(
        "Klystrack",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* Create renderer */
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* Initialize audio */
    SDL_AudioSpec desired, obtained;
    memset(&desired, 0, sizeof(desired));
    desired.freq = SAMPLE_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = AUDIO_BUFFER;
    desired.callback = audio_callback;
    desired.userdata = NULL;

    if (SDL_OpenAudio(&desired, &obtained) < 0) {
        printf("Warning: SDL_OpenAudio failed: %s\n", SDL_GetAudioError());
        printf("Continuing without audio...\n");
    }

    /* Initialize tracker state */
    init_instruments();
    init_demo_pattern();

    /* Initialize channel states */
    for (int i = 0; i < NUM_CHANNELS; i++) {
        memset(&channels[i], 0, sizeof(channel_state_t));
        channels[i].lfsr = 0x1234 + i;
    }

    /* Start audio playback */
    SDL_PauseAudio(0);

    printf("Ready! Press SPACE to play, Q to quit\n");

    /* Main loop */
    int running = 1;
    Uint32 last_tick = SDL_GetTicks();

    while (running) {
        /* Handle events */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            running = handle_input(&event);
        }

        /* Update playback */
        if (is_playing) {
            Uint32 now = SDL_GetTicks();
            double delta = (now - last_tick) / 1000.0;
            last_tick = now;

            /* Calculate ticks per second based on tempo and speed */
            double ticks_per_second = (tempo * 24.0) / 60.0;
            play_tick += delta * ticks_per_second;

            while (play_tick >= speed) {
                play_tick -= speed;
                play_row++;
                if (play_row >= PATTERN_LENGTH) {
                    play_row = 0;
                }
                process_row(current_pattern, play_row);
            }
        } else {
            last_tick = SDL_GetTicks();
        }

        /* Pump audio manually (VOS doesn't have audio threads) */
        SDL_PumpAudio();

        /* Draw UI */
        draw_ui(renderer);
        SDL_RenderPresent(renderer);

        /* Small delay to avoid busy-waiting */
        SDL_Delay(16);
    }

    /* Cleanup */
    SDL_CloseAudio();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Goodbye!\n");
    return 0;
}
