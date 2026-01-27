// Example: stately.h - Minimal Finite State Machine
// Compile: tcc ex_stately.c -o ex_stately

#include <stdio.h>

#include "../stately.h"

// Player states
enum PlayerState {
    PLAYER_IDLE,
    PLAYER_WALKING,
    PLAYER_RUNNING,
    PLAYER_JUMPING,
    PLAYER_FALLING,
    PLAYER_DEAD,
    PLAYER_STATE_COUNT
};

// Input events
enum PlayerEvent {
    INPUT_NONE,
    INPUT_MOVE,
    INPUT_STOP,
    INPUT_RUN,
    INPUT_JUMP,
    INPUT_LAND,
    INPUT_DIE,
    INPUT_RESPAWN,
    PLAYER_EVENT_COUNT
};

const char* state_names[] = {"IDLE", "WALKING", "RUNNING", "JUMPING", "FALLING", "DEAD"};
const char* event_names[] = {"NONE", "MOVE", "STOP", "RUN", "JUMP", "LAND", "DIE", "RESPAWN"};

// Simple FSM structure
typedef struct {
    int state;
    int prev_state;
    int frame_count;  // How long in current state
} PlayerFSM;

// Transition function
int player_transition(PlayerFSM* fsm, int event) {
    int old_state = fsm->state;
    int new_state = old_state;

    switch (fsm->state) {
        case PLAYER_IDLE:
            if (event == INPUT_MOVE) new_state = PLAYER_WALKING;
            else if (event == INPUT_JUMP) new_state = PLAYER_JUMPING;
            else if (event == INPUT_DIE) new_state = PLAYER_DEAD;
            break;

        case PLAYER_WALKING:
            if (event == INPUT_STOP) new_state = PLAYER_IDLE;
            else if (event == INPUT_RUN) new_state = PLAYER_RUNNING;
            else if (event == INPUT_JUMP) new_state = PLAYER_JUMPING;
            else if (event == INPUT_DIE) new_state = PLAYER_DEAD;
            break;

        case PLAYER_RUNNING:
            if (event == INPUT_STOP) new_state = PLAYER_IDLE;
            else if (event == INPUT_MOVE) new_state = PLAYER_WALKING;
            else if (event == INPUT_JUMP) new_state = PLAYER_JUMPING;
            else if (event == INPUT_DIE) new_state = PLAYER_DEAD;
            break;

        case PLAYER_JUMPING:
            // After some frames, transition to falling
            if (fsm->frame_count > 10) new_state = PLAYER_FALLING;
            else if (event == INPUT_DIE) new_state = PLAYER_DEAD;
            break;

        case PLAYER_FALLING:
            if (event == INPUT_LAND) new_state = PLAYER_IDLE;
            else if (event == INPUT_DIE) new_state = PLAYER_DEAD;
            break;

        case PLAYER_DEAD:
            if (event == INPUT_RESPAWN) new_state = PLAYER_IDLE;
            break;
    }

    if (new_state != old_state) {
        fsm->prev_state = old_state;
        fsm->state = new_state;
        fsm->frame_count = 0;
        return 1;  // State changed
    }
    return 0;  // No change
}

// Update function (called every frame)
void player_update(PlayerFSM* fsm) {
    fsm->frame_count++;

    // Auto-transitions based on time
    if (fsm->state == PLAYER_JUMPING && fsm->frame_count > 10) {
        player_transition(fsm, INPUT_NONE);
    }
}

int main(void) {
    printf("=== stately.h (Minimal FSM) Example ===\n\n");

    printf("Simple player state machine for platformer game.\n\n");

    // Initialize FSM
    PlayerFSM player = {PLAYER_IDLE, PLAYER_IDLE, 0};
    printf("Initial state: %s\n\n", state_names[player.state]);

    // Simulate gameplay
    printf("--- Gameplay Simulation ---\n\n");

    // Sequence of inputs
    struct { int event; int frames; } inputs[] = {
        {INPUT_MOVE, 5},      // Start walking
        {INPUT_RUN, 3},       // Start running
        {INPUT_JUMP, 15},     // Jump (will auto-transition to falling)
        {INPUT_LAND, 1},      // Land
        {INPUT_MOVE, 3},      // Walk again
        {INPUT_STOP, 2},      // Stop
        {INPUT_DIE, 1},       // Oh no!
        {INPUT_RESPAWN, 1},   // Try again
    };

    int total_frames = 0;
    for (int i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
        int event = inputs[i].event;
        int duration = inputs[i].frames;

        printf("Frame %3d: Event %s\n", total_frames, event_names[event]);

        int changed = player_transition(&player, event);
        if (changed) {
            printf("           State: %s -> %s\n",
                   state_names[player.prev_state], state_names[player.state]);
        }

        // Simulate frames
        for (int f = 0; f < duration; f++) {
            player_update(&player);
            total_frames++;

            // Check for auto-transitions
            if (player.state == PLAYER_JUMPING && player.frame_count > 10) {
                int old = player.state;
                player.state = PLAYER_FALLING;
                player.prev_state = old;
                player.frame_count = 0;
                printf("Frame %3d: [Auto] %s -> %s (gravity)\n",
                       total_frames, state_names[old], state_names[player.state]);
            }
        }
    }

    printf("\nFinal state: %s (after %d frames)\n", state_names[player.state], total_frames);

    // State duration tracking
    printf("\n--- State Duration Tracking ---\n");
    printf("Current state: %s\n", state_names[player.state]);
    printf("Frames in state: %d\n", player.frame_count);
    printf("Previous state: %s\n", state_names[player.prev_state]);

    printf("\nDone!\n");
    return 0;
}
