// Example: StateMachine.h - Event-Driven State Machine
// Compile: tcc ex_StateMachine.c ../StateMachine.c -o ex_StateMachine
// Note: Uses DataTypes.h and Fault.h stubs from gameResources

#include <stdio.h>
#include <string.h>

// Include the stub headers first
#include "../DataTypes.h"
#include "../Fault.h"

// Now we can include StateMachine
// Note: In a real project, you may need to adapt StateMachine.h
// to remove the sm_allocator dependency or provide a stub

// For this example, we'll demonstrate the concepts manually
// since StateMachine.h has complex dependencies

// States for a door
typedef enum {
    ST_CLOSED,
    ST_OPENING,
    ST_OPEN,
    ST_CLOSING,
    ST_COUNT
} DoorState;

// Events
typedef enum {
    EV_OPEN_BUTTON,
    EV_CLOSE_BUTTON,
    EV_SENSOR_CLEAR,
    EV_SENSOR_BLOCKED,
    EV_TIMER_DONE,
    EV_COUNT
} DoorEvent;

const char* state_names[] = {"CLOSED", "OPENING", "OPEN", "CLOSING"};
const char* event_names[] = {"OPEN_BUTTON", "CLOSE_BUTTON", "SENSOR_CLEAR", "SENSOR_BLOCKED", "TIMER_DONE"};

// Transition table: [current_state][event] -> next_state (-1 = invalid)
int transitions[ST_COUNT][EV_COUNT] = {
    // CLOSED state
    {ST_OPENING, -1, -1, -1, -1},  // OPEN_BUTTON -> OPENING

    // OPENING state
    {-1, -1, ST_OPEN, -1, ST_OPEN},  // SENSOR_CLEAR or TIMER -> OPEN

    // OPEN state
    {-1, ST_CLOSING, -1, -1, ST_CLOSING},  // CLOSE_BUTTON or TIMER -> CLOSING

    // CLOSING state
    {ST_OPENING, -1, ST_CLOSED, ST_OPENING, -1}  // Various transitions
};

// Current state
DoorState current_state = ST_CLOSED;

// State machine event handler
BOOL handle_event(DoorEvent event) {
    int next = transitions[current_state][event];

    if (next < 0) {
        printf("  Event %s ignored in state %s\n",
               event_names[event], state_names[current_state]);
        return FALSE;
    }

    DoorState old_state = current_state;
    current_state = (DoorState)next;

    printf("  Transition: %s -> %s (on %s)\n",
           state_names[old_state], state_names[current_state], event_names[event]);

    return TRUE;
}

// Entry actions for states
void on_enter_state(DoorState state) {
    switch (state) {
        case ST_OPENING:
            printf("    [Action] Motor: OPENING\n");
            break;
        case ST_OPEN:
            printf("    [Action] Motor: STOPPED, Start auto-close timer\n");
            break;
        case ST_CLOSING:
            printf("    [Action] Motor: CLOSING\n");
            break;
        case ST_CLOSED:
            printf("    [Action] Motor: STOPPED, Door locked\n");
            break;
        default:
            break;
    }
}

int main(void) {
    printf("=== StateMachine.h Concepts Example ===\n\n");

    printf("This demonstrates event-driven state machine patterns.\n");
    printf("(Using simplified implementation due to library dependencies)\n\n");

    printf("Automatic Door State Machine:\n");
    printf("  States: CLOSED, OPENING, OPEN, CLOSING\n");
    printf("  Events: OPEN_BUTTON, CLOSE_BUTTON, SENSOR_CLEAR, SENSOR_BLOCKED, TIMER_DONE\n\n");

    printf("Initial state: %s\n\n", state_names[current_state]);

    // Simulate door operation
    printf("--- Simulation: Normal door cycle ---\n\n");

    DoorEvent scenario[] = {
        EV_OPEN_BUTTON,    // Someone presses open
        EV_TIMER_DONE,     // Door finishes opening
        EV_TIMER_DONE,     // Auto-close timer expires
        EV_SENSOR_BLOCKED, // Someone walks through while closing!
        EV_SENSOR_CLEAR,   // Path clear again
        EV_TIMER_DONE,     // Auto-close timer
        EV_SENSOR_CLEAR,   // Door finishes closing
    };

    for (int i = 0; i < sizeof(scenario)/sizeof(scenario[0]); i++) {
        printf("Event: %s\n", event_names[scenario[i]]);
        DoorState old = current_state;
        if (handle_event(scenario[i])) {
            if (current_state != old) {
                on_enter_state(current_state);
            }
        }
        printf("\n");
    }

    printf("Final state: %s\n\n", state_names[current_state]);

    // Show transition table
    printf("--- Transition Table ---\n");
    printf("%-10s |", "State");
    for (int e = 0; e < EV_COUNT; e++) {
        printf(" %-14s", event_names[e]);
    }
    printf("\n");
    for (int i = 0; i < 11 + EV_COUNT * 15; i++) printf("-");
    printf("\n");

    for (int s = 0; s < ST_COUNT; s++) {
        printf("%-10s |", state_names[s]);
        for (int e = 0; e < EV_COUNT; e++) {
            int next = transitions[s][e];
            if (next >= 0) {
                printf(" %-14s", state_names[next]);
            } else {
                printf(" %-14s", "-");
            }
        }
        printf("\n");
    }

    printf("\nDone!\n");
    return 0;
}
