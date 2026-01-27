// Example: hsm.h - Hierarchical State Machine
// Compile: tcc ex_hsm.c hsm.c -o ex_hsm
// Note: Requires hsm.c to be compiled alongside

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "../hsm_config.h"
#include "../hsm.h"

// States for a character controller
typedef enum {
    // Top level
    ST_ALIVE,
    ST_DEAD,
    // Children of ALIVE
    ST_IDLE,
    ST_MOVING,
    // Children of MOVING
    ST_WALKING,
    ST_RUNNING,
    ST_COUNT
} CharacterState;

// Events
typedef enum {
    EV_MOVE,
    EV_STOP,
    EV_RUN,
    EV_WALK,
    EV_DIE,
    EV_RESPAWN,
    EV_COUNT
} CharacterEvent;

const char* state_names[] = {"ALIVE", "DEAD", "IDLE", "MOVING", "WALKING", "RUNNING"};
const char* event_names[] = {"MOVE", "STOP", "RUN", "WALK", "DIE", "RESPAWN"};

// State handlers
state_machine_result_t on_idle_handler(state_machine_t* sm, uint32_t event) {
    printf("  [IDLE] received event: %s\n", event_names[event]);
    if (event == EV_MOVE) {
        return (state_machine_result_t){.state = ST_WALKING, .consumed = true};
    }
    if (event == EV_DIE) {
        return (state_machine_result_t){.state = ST_DEAD, .consumed = true};
    }
    return (state_machine_result_t){.state = ST_IDLE, .consumed = false};
}

state_machine_result_t on_walking_handler(state_machine_t* sm, uint32_t event) {
    printf("  [WALKING] received event: %s\n", event_names[event]);
    if (event == EV_STOP) {
        return (state_machine_result_t){.state = ST_IDLE, .consumed = true};
    }
    if (event == EV_RUN) {
        return (state_machine_result_t){.state = ST_RUNNING, .consumed = true};
    }
    return (state_machine_result_t){.state = ST_WALKING, .consumed = false};
}

state_machine_result_t on_running_handler(state_machine_t* sm, uint32_t event) {
    printf("  [RUNNING] received event: %s\n", event_names[event]);
    if (event == EV_WALK) {
        return (state_machine_result_t){.state = ST_WALKING, .consumed = true};
    }
    if (event == EV_STOP) {
        return (state_machine_result_t){.state = ST_IDLE, .consumed = true};
    }
    return (state_machine_result_t){.state = ST_RUNNING, .consumed = false};
}

state_machine_result_t on_dead_handler(state_machine_t* sm, uint32_t event) {
    printf("  [DEAD] received event: %s\n", event_names[event]);
    if (event == EV_RESPAWN) {
        return (state_machine_result_t){.state = ST_IDLE, .consumed = true};
    }
    return (state_machine_result_t){.state = ST_DEAD, .consumed = false};
}

int main(void) {
    printf("=== hsm.h (Hierarchical State Machine) Example ===\n\n");

    printf("Character State Hierarchy:\n");
    printf("  ALIVE\n");
    printf("    IDLE\n");
    printf("    MOVING\n");
    printf("      WALKING\n");
    printf("      RUNNING\n");
    printf("  DEAD\n\n");

    // For this simplified example, we'll demonstrate the concept
    // without the full HSM setup (which requires more boilerplate)

    printf("--- Simulating State Transitions ---\n");

    CharacterState current = ST_IDLE;
    CharacterEvent events[] = {EV_MOVE, EV_RUN, EV_WALK, EV_STOP, EV_DIE, EV_RESPAWN};

    printf("Starting state: %s\n\n", state_names[current]);

    for (int i = 0; i < 6; i++) {
        CharacterEvent e = events[i];
        printf("Event: %s\n", event_names[e]);

        CharacterState next = current;

        // Simple state transition logic (simplified HSM behavior)
        switch (current) {
            case ST_IDLE:
                if (e == EV_MOVE) next = ST_WALKING;
                else if (e == EV_DIE) next = ST_DEAD;
                break;
            case ST_WALKING:
                if (e == EV_STOP) next = ST_IDLE;
                else if (e == EV_RUN) next = ST_RUNNING;
                else if (e == EV_DIE) next = ST_DEAD;
                break;
            case ST_RUNNING:
                if (e == EV_WALK) next = ST_WALKING;
                else if (e == EV_STOP) next = ST_IDLE;
                else if (e == EV_DIE) next = ST_DEAD;
                break;
            case ST_DEAD:
                if (e == EV_RESPAWN) next = ST_IDLE;
                break;
            default:
                break;
        }

        if (next != current) {
            printf("  Transition: %s -> %s\n", state_names[current], state_names[next]);
            current = next;
        } else {
            printf("  No transition (stayed in %s)\n", state_names[current]);
        }
        printf("\n");
    }

    printf("Final state: %s\n", state_names[current]);
    printf("\nDone!\n");
    return 0;
}
