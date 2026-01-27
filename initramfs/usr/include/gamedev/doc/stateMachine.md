# stateMachine.h - Feature-Rich Finite State Machine

A feature-rich, yet simple finite state machine (FSM) implementation in C with support for nested/grouped states, guards, and entry/exit actions.

## Overview

`stateMachine.h` provides a robust FSM implementation designed for scenarios where a simple switch statement isn't enough. It supports hierarchical state organization through parent states, guarded transitions, events with payloads, and comprehensive lifecycle actions.

## Original Source

**Repository**: https://github.com/misje/stateMachine

**Author**: Andreas Misje

**Date**: March 27, 2013

**License**: MIT License

**Documentation**: https://misje.github.io/stateMachine/

## Features

- **Grouped/nested states** - Parent-child state relationships for hierarchical organization
- **Guarded transitions** - Conditional transitions based on guard functions
- **Events with payload** - Events can carry arbitrary data
- **Entry/exit actions** - State lifecycle callbacks
- **Transition actions** - Actions executed during state changes
- **User-defined state data** - Attach custom data to each state
- **Error state handling** - Dedicated error state for fault conditions
- **Previous state tracking** - Access to the last state for history

## API Reference

### Types

#### Event

```c
struct event {
    int type;        // User-defined event type
    void *data;      // Optional event payload
};
```

#### Transition

```c
struct transition {
    int eventType;                                          // Event that triggers this transition
    void *condition;                                        // Data passed to guard function
    bool (*guard)(void *condition, struct event *event);    // Guard condition (optional)
    void (*action)(void *currentStateData, struct event *event, void *newStateData);  // Transition action (optional)
    struct state *nextState;                                // Target state (required)
};
```

#### State

```c
struct state {
    struct state *parentState;                              // Parent state (NULL if top-level)
    struct state *entryState;                               // Entry point for group states
    struct transition *transitions;                         // Array of transitions
    size_t numTransitions;                                  // Number of transitions (0 = final state)
    void *data;                                             // User-defined state data
    void (*entryAction)(void *stateData, struct event *event);  // Entry callback
    void (*exitAction)(void *stateData, struct event *event);   // Exit callback
};
```

#### State Machine

```c
struct stateMachine {
    struct state *currentState;    // Current state
    struct state *previousState;   // Previous state (for history)
    struct state *errorState;      // Error state for fault handling
};
```

### Functions

```c
// Initialize state machine with initial and error states
void stateM_init(struct stateMachine *stateMachine,
                 struct state *initialState,
                 struct state *errorState);

// Handle an event, potentially triggering a transition
// Returns: stateM_handleEventRetVals enum value
int stateM_handleEvent(struct stateMachine *stateMachine,
                       struct event *event);

// Get current state
struct state *stateM_currentState(struct stateMachine *stateMachine);

// Get previous state
struct state *stateM_previousState(struct stateMachine *stateMachine);

// Check if state machine has reached a final state
bool stateM_stopped(struct stateMachine *stateMachine);
```

### Return Values

```c
enum stateM_handleEventRetVals {
    stateM_errArg = -2,           // Invalid arguments
    stateM_errorStateReached,     // Error state was entered
    stateM_stateChanged,          // Transitioned to new state
    stateM_stateLoopSelf,         // Returned to same state
    stateM_noStateChange,         // Event didn't cause transition
    stateM_finalStateReached,     // Final state reached
};
```

## Usage Example

```c
#include <stdio.h>
#include <stdbool.h>
#include "stateMachine.h"

// Event types
enum { EVENT_COIN, EVENT_PUSH };

// Forward declarations
struct state lockedState, unlockedState, errorState;

// Guard: check if coin is valid
bool isValidCoin(void *condition, struct event *event) {
    int *coinValue = (int*)event->data;
    return coinValue && *coinValue >= 25;  // Need at least 25 cents
}

// Entry action for locked state
void onEnterLocked(void *stateData, struct event *event) {
    printf("Turnstile LOCKED\n");
}

// Entry action for unlocked state
void onEnterUnlocked(void *stateData, struct event *event) {
    printf("Turnstile UNLOCKED - please pass through\n");
}

// Transition action
void onCoinInserted(void *currentData, struct event *event, void *newData) {
    int *coinValue = (int*)event->data;
    printf("Coin inserted: %d cents\n", coinValue ? *coinValue : 0);
}

// Transitions from locked state
struct transition lockedTransitions[] = {
    {
        .eventType = EVENT_COIN,
        .guard = isValidCoin,
        .action = onCoinInserted,
        .nextState = &unlockedState,
    },
    // EVENT_PUSH while locked does nothing (no transition)
};

// Transitions from unlocked state
struct transition unlockedTransitions[] = {
    {
        .eventType = EVENT_PUSH,
        .nextState = &lockedState,  // Lock after someone passes
    },
};

// State definitions
struct state lockedState = {
    .parentState = NULL,
    .entryState = NULL,
    .transitions = lockedTransitions,
    .numTransitions = 1,
    .entryAction = onEnterLocked,
};

struct state unlockedState = {
    .parentState = NULL,
    .entryState = NULL,
    .transitions = unlockedTransitions,
    .numTransitions = 1,
    .entryAction = onEnterUnlocked,
};

struct state errorState = {
    .numTransitions = 0,  // Final state
};

int main(void) {
    struct stateMachine turnstile;
    stateM_init(&turnstile, &lockedState, &errorState);

    printf("=== Turnstile State Machine ===\n\n");
    onEnterLocked(NULL, NULL);

    // Try to push (should fail - locked)
    struct event pushEvent = {EVENT_PUSH, NULL};
    int result = stateM_handleEvent(&turnstile, &pushEvent);
    printf("Push while locked: %s\n\n",
           result == stateM_noStateChange ? "blocked" : "passed");

    // Insert invalid coin (10 cents)
    int smallCoin = 10;
    struct event coinEvent = {EVENT_COIN, &smallCoin};
    result = stateM_handleEvent(&turnstile, &coinEvent);
    printf("Small coin: %s\n\n",
           result == stateM_noStateChange ? "rejected" : "accepted");

    // Insert valid coin (25 cents)
    int validCoin = 25;
    coinEvent.data = &validCoin;
    result = stateM_handleEvent(&turnstile, &coinEvent);
    printf("Valid coin: %s\n\n",
           result == stateM_stateChanged ? "accepted" : "rejected");

    // Push through
    result = stateM_handleEvent(&turnstile, &pushEvent);
    printf("Push: %s\n",
           result == stateM_stateChanged ? "passed through" : "blocked");

    return 0;
}
```

## Grouped/Nested States Example

```c
// Parent state with common transitions
struct state operatingState = {
    .parentState = NULL,
    .entryState = &idleState,      // Default child
    .transitions = emergencyTransitions,  // Emergency stop available from any child
    .numTransitions = 1,
};

// Child states inherit parent's transitions
struct state idleState = {
    .parentState = &operatingState,
    .transitions = idleTransitions,
    .numTransitions = 2,
};

struct state runningState = {
    .parentState = &operatingState,
    .transitions = runningTransitions,
    .numTransitions = 2,
};

// If idleState doesn't handle an event, it bubbles up to operatingState
```

## State Types

### Normal State
Has transitions and optionally a parent:

```c
struct state normalState = {
    .parentState = &groupState,  // Or NULL
    .transitions = myTransitions,
    .numTransitions = 3,
    .data = &myStateData,
    .entryAction = onEnter,
    .exitAction = onExit,
};
```

### Group/Parent State
A state that other states reference as their parent:

```c
struct state groupState = {
    .entryState = &defaultChildState,  // Entry point when entering group
    .transitions = commonTransitions,   // Shared by all children
    .numTransitions = 1,
};
```

### Final State
A state with no transitions (terminates the machine):

```c
struct state finalState = {
    .transitions = NULL,
    .numTransitions = 0,
};
```

## Build Instructions

Requires both header and source file:

```bash
tcc myprogram.c stateMachine.c -o myprogram
```

## VOS/TCC Compatibility Notes

- **TCC Compatible**: Yes - uses standard C99 features
- **Files Required**: `stateMachine.h` and `stateMachine.c`
- **Dependencies**: `<stddef.h>`, `<stdbool.h>`
- **Memory**: All state data can be stack or heap allocated
- **Build**: `tcc myfile.c stateMachine.c -o myfile`

## Implementation Notes

- Entry action is NOT called on initial state during `stateM_init()`
- Entry/exit actions are NOT called when a state returns to itself
- If event doesn't match any transition, it bubbles to parent state
- Transition without `nextState` triggers error state
- Error state should be a final state (numTransitions = 0)
- Guard receives both the transition's condition and the full event
