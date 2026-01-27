// Example: sm.h - Simple State Machine
// Compile: tcc ex_sm.c -o ex_sm

#include <stdio.h>

#define SM_IMPLEMENTATION
#include "../sm.h"

// Game states
typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAMEOVER,
    STATE_COUNT
} GameState;

// Events that trigger transitions
typedef enum {
    EVENT_START,
    EVENT_PAUSE,
    EVENT_RESUME,
    EVENT_DIE,
    EVENT_RESTART,
    EVENT_COUNT
} GameEvent;

// State names for printing
const char* state_names[] = {"MENU", "PLAYING", "PAUSED", "GAMEOVER"};
const char* event_names[] = {"START", "PAUSE", "RESUME", "DIE", "RESTART"};

// Callbacks for state entry/exit
void on_enter_menu(void* ctx) {
    printf("  [Enter MENU] - Press START to begin!\n");
}

void on_exit_menu(void* ctx) {
    printf("  [Exit MENU]\n");
}

void on_enter_playing(void* ctx) {
    printf("  [Enter PLAYING] - Game started! Good luck!\n");
}

void on_enter_paused(void* ctx) {
    printf("  [Enter PAUSED] - Game paused.\n");
}

void on_enter_gameover(void* ctx) {
    printf("  [Enter GAMEOVER] - You died! Press RESTART.\n");
}

int main(void) {
    printf("=== sm.h (State Machine) Example ===\n\n");

    // Create state machine
    sm_t sm;
    sm_init(&sm, STATE_COUNT, EVENT_COUNT, STATE_MENU);

    // Define transitions: sm_add_transition(sm, from_state, event, to_state)
    sm_add_transition(&sm, STATE_MENU,     EVENT_START,   STATE_PLAYING);
    sm_add_transition(&sm, STATE_PLAYING,  EVENT_PAUSE,   STATE_PAUSED);
    sm_add_transition(&sm, STATE_PLAYING,  EVENT_DIE,     STATE_GAMEOVER);
    sm_add_transition(&sm, STATE_PAUSED,   EVENT_RESUME,  STATE_PLAYING);
    sm_add_transition(&sm, STATE_GAMEOVER, EVENT_RESTART, STATE_MENU);

    // Set callbacks (optional)
    sm_set_enter_callback(&sm, STATE_MENU,     on_enter_menu);
    sm_set_exit_callback(&sm, STATE_MENU,      on_exit_menu);
    sm_set_enter_callback(&sm, STATE_PLAYING,  on_enter_playing);
    sm_set_enter_callback(&sm, STATE_PAUSED,   on_enter_paused);
    sm_set_enter_callback(&sm, STATE_GAMEOVER, on_enter_gameover);

    printf("State machine configured with %d states, %d events.\n\n", STATE_COUNT, EVENT_COUNT);

    // Initial state
    printf("Initial state: %s\n\n", state_names[sm_get_state(&sm)]);
    on_enter_menu(NULL);

    // Simulate game flow
    GameEvent events[] = {
        EVENT_START,   // MENU -> PLAYING
        EVENT_PAUSE,   // PLAYING -> PAUSED
        EVENT_RESUME,  // PAUSED -> PLAYING
        EVENT_DIE,     // PLAYING -> GAMEOVER
        EVENT_RESTART  // GAMEOVER -> MENU
    };

    printf("\n--- Simulating Game Flow ---\n");
    for (int i = 0; i < 5; i++) {
        GameEvent e = events[i];
        GameState old_state = sm_get_state(&sm);

        printf("\nEvent: %s\n", event_names[e]);

        bool valid = sm_send_event(&sm, e, NULL);

        if (valid) {
            GameState new_state = sm_get_state(&sm);
            printf("  Transition: %s -> %s\n", state_names[old_state], state_names[new_state]);
        } else {
            printf("  Invalid transition from %s!\n", state_names[old_state]);
        }
    }

    // Try invalid transition
    printf("\n--- Testing Invalid Transition ---\n");
    printf("Current state: %s\n", state_names[sm_get_state(&sm)]);
    printf("Sending DIE event (invalid from MENU)...\n");
    bool valid = sm_send_event(&sm, EVENT_DIE, NULL);
    printf("Transition valid: %s\n", valid ? "YES" : "NO");

    printf("\nDone!\n");
    return 0;
}
