// Example: stateMachine.h - Feature-Rich FSM with Guards
// Compile: tcc ex_stateMachine.c ../stateMachine.c -o ex_stateMachine

#include <stdio.h>
#include <stdbool.h>

#include "../stateMachine.h"

// Traffic light states
enum TrafficState {
    STATE_RED,
    STATE_GREEN,
    STATE_YELLOW,
    STATE_FLASHING,
    STATE_COUNT
};

// Events
enum TrafficEvent {
    EVENT_TIMER,
    EVENT_EMERGENCY,
    EVENT_CLEAR,
    EVENT_NIGHT_MODE,
    EVENT_COUNT
};

const char* state_str[] = {"RED", "GREEN", "YELLOW", "FLASHING"};
const char* event_str[] = {"TIMER", "EMERGENCY", "CLEAR", "NIGHT_MODE"};

// Context data passed to guards and actions
typedef struct {
    int cars_waiting;
    int pedestrians_waiting;
    bool emergency_vehicle;
    int hour;  // 0-23
} TrafficContext;

// Guard: Only change if cars are waiting
bool guard_cars_waiting(void* condition, struct event* event) {
    TrafficContext* ctx = (TrafficContext*)condition;
    printf("    [Guard] Cars waiting: %d\n", ctx->cars_waiting);
    return ctx->cars_waiting > 0;
}

// Guard: Night mode only between 11pm-5am
bool guard_night_hours(void* condition, struct event* event) {
    TrafficContext* ctx = (TrafficContext*)condition;
    bool is_night = (ctx->hour >= 23 || ctx->hour < 5);
    printf("    [Guard] Hour: %d, Night mode: %s\n", ctx->hour, is_night ? "YES" : "NO");
    return is_night;
}

// Entry action
void action_entry_red(void* stateData, struct event* event) {
    printf("    [Action] RED light ON - Stop all traffic\n");
}

void action_entry_green(void* stateData, struct event* event) {
    printf("    [Action] GREEN light ON - Traffic may proceed\n");
}

void action_entry_yellow(void* stateData, struct event* event) {
    printf("    [Action] YELLOW light ON - Prepare to stop\n");
}

void action_entry_flashing(void* stateData, struct event* event) {
    printf("    [Action] FLASHING mode - Proceed with caution\n");
}

// Exit action
void action_exit_generic(void* stateData, struct event* event) {
    printf("    [Action] Light OFF\n");
}

int main(void) {
    printf("=== stateMachine.h (Feature-Rich FSM) Example ===\n\n");

    printf("Traffic Light Controller with guards and actions.\n\n");

    // Note: The actual stateMachine.h API is more complex.
    // This example demonstrates the CONCEPTS of:
    // - Guards (conditions that must be true for transition)
    // - Entry/Exit actions
    // - Event-driven state changes

    // Simplified demonstration
    TrafficContext ctx = {
        .cars_waiting = 3,
        .pedestrians_waiting = 1,
        .emergency_vehicle = false,
        .hour = 14  // 2 PM
    };

    int current_state = STATE_RED;

    printf("Initial state: %s\n", state_str[current_state]);
    printf("Context: %d cars, %d pedestrians, hour=%d\n\n",
           ctx.cars_waiting, ctx.pedestrians_waiting, ctx.hour);

    // Simulate traffic light cycle
    printf("--- Traffic Light Cycle ---\n\n");

    struct { int event; } events[] = {
        {EVENT_TIMER},      // RED -> GREEN
        {EVENT_TIMER},      // GREEN -> YELLOW
        {EVENT_TIMER},      // YELLOW -> RED
        {EVENT_NIGHT_MODE}, // Attempt night mode (should fail - not night)
        {EVENT_EMERGENCY},  // Emergency!
        {EVENT_CLEAR},      // Emergency over
    };

    for (int i = 0; i < 6; i++) {
        int event = events[i].event;
        printf("Event: %s\n", event_str[event]);

        int old_state = current_state;
        int new_state = old_state;
        bool transition_allowed = true;

        // State machine logic with guards
        switch (current_state) {
            case STATE_RED:
                if (event == EVENT_TIMER) {
                    // Guard: must have cars waiting
                    if (guard_cars_waiting(&ctx, NULL)) {
                        action_exit_generic(NULL, NULL);
                        new_state = STATE_GREEN;
                        action_entry_green(NULL, NULL);
                    } else {
                        transition_allowed = false;
                    }
                } else if (event == EVENT_NIGHT_MODE) {
                    if (guard_night_hours(&ctx, NULL)) {
                        new_state = STATE_FLASHING;
                        action_entry_flashing(NULL, NULL);
                    } else {
                        printf("    [Rejected] Not night hours\n");
                        transition_allowed = false;
                    }
                } else if (event == EVENT_EMERGENCY) {
                    new_state = STATE_FLASHING;
                    action_entry_flashing(NULL, NULL);
                }
                break;

            case STATE_GREEN:
                if (event == EVENT_TIMER) {
                    action_exit_generic(NULL, NULL);
                    new_state = STATE_YELLOW;
                    action_entry_yellow(NULL, NULL);
                } else if (event == EVENT_EMERGENCY) {
                    new_state = STATE_FLASHING;
                }
                break;

            case STATE_YELLOW:
                if (event == EVENT_TIMER) {
                    action_exit_generic(NULL, NULL);
                    new_state = STATE_RED;
                    action_entry_red(NULL, NULL);
                } else if (event == EVENT_EMERGENCY) {
                    new_state = STATE_FLASHING;
                }
                break;

            case STATE_FLASHING:
                if (event == EVENT_CLEAR) {
                    action_exit_generic(NULL, NULL);
                    new_state = STATE_RED;
                    action_entry_red(NULL, NULL);
                }
                break;
        }

        if (new_state != old_state) {
            printf("  Transition: %s -> %s\n", state_str[old_state], state_str[new_state]);
            current_state = new_state;
        } else if (!transition_allowed) {
            printf("  Transition blocked by guard\n");
        } else {
            printf("  No transition for this event\n");
        }
        printf("\n");
    }

    printf("Final state: %s\n", state_str[current_state]);

    // Now test night mode
    printf("\n--- Testing Night Mode (hour=23) ---\n\n");
    ctx.hour = 23;
    current_state = STATE_RED;

    printf("Event: NIGHT_MODE\n");
    if (guard_night_hours(&ctx, NULL)) {
        current_state = STATE_FLASHING;
        action_entry_flashing(NULL, NULL);
        printf("  Transition: RED -> FLASHING\n");
    }

    printf("\nDone!\n");
    return 0;
}
