# hsm.h - Hierarchical State Machine

A minimalist UML State Machine framework for finite state machines (FSM) and hierarchical state machines (HSM) in C.

## Overview

`hsm.h` is a lightweight state machine framework designed for embedded systems. It supports both flat finite state machines and hierarchical (nested) state machines based on UML statecharts. The framework is CPU and OS independent with minimal memory footprint.

## Original Source

**Repository**: https://github.com/kiishor/UML-State-Machine-in-C

**Author**: Nandkishor Biradar (kiishor)

**Date**: December 1, 2018

**License**: MIT License

**Reference**: Based on "Practical UML Statecharts in C/C++, 2nd Ed" by Miro Samek

## Features

- **Minimalist API** - Only 3 functions to learn
- **Dual mode** - Supports both FSM and HSM via compile-time configuration
- **Memory efficient** - 116 bytes (FSM) / 424 bytes (HSM) code memory
- **Zero data memory** - Framework adds no runtime data overhead
- **Const-compatible** - State definitions can reside in ROM
- **Event priority** - Array-index-based priority dispatching
- **Run-to-completion** - Non-preemptive execution model
- **Optional logging** - Built-in state machine logger support
- **Platform independent** - CPU and OS agnostic

## API Reference

### Result Codes

```c
typedef enum {
    EVENT_HANDLED,      // Event handled successfully
    EVENT_UN_HANDLED,   // Event could not be handled
    TRIGGERED_TO_SELF,  // Handler posted new event to itself
} state_machine_result_t;
```

### Types

```c
// Forward declarations
typedef struct finite_state state_t;        // In FSM mode
typedef struct hierarchical_state state_t;  // In HSM mode
typedef struct state_machine_t state_machine_t;

// Handler function signature
typedef state_machine_result_t (*state_handler)(state_machine_t* const State);

// Optional logger callbacks
typedef void (*state_machine_event_logger)(uint32_t state_machine, uint32_t state, uint32_t event);
typedef void (*state_machine_result_logger)(uint32_t state, state_machine_result_t result);
```

### Finite State Structure

```c
struct finite_state {
    state_handler Handler;    // Event handler function (required)
    state_handler Entry;      // Entry action (optional)
    state_handler Exit;       // Exit action (optional)
#if STATE_MACHINE_LOGGER
    uint32_t Id;              // Unique state identifier for logging
#endif
};
```

### Hierarchical State Structure

```c
struct hierarchical_state {
    state_handler Handler;           // Event handler function (required)
    state_handler Entry;             // Entry action (optional)
    state_handler Exit;              // Exit action (optional)
#if STATE_MACHINE_LOGGER
    uint32_t Id;                     // Unique state identifier
#endif
    const state_t* const Parent;     // Parent state (NULL for top-level)
    const state_t* const Node;       // Default child state (optional)
    uint32_t Level;                  // Hierarchy depth from root (0 = top)
};
```

### State Machine Structure

```c
struct state_machine_t {
    uint32_t Event;           // Pending event for state machine
    const state_t* State;     // Current state pointer
};
```

### Functions

```c
// Dispatch pending events across array of state machines
// Returns EVENT_HANDLED when all events processed, EVENT_UN_HANDLED on error
state_machine_result_t dispatch_event(
    state_machine_t* const pState_Machine[],
    uint32_t quantity
#if STATE_MACHINE_LOGGER
    , state_machine_event_logger event_logger
    , state_machine_result_logger result_logger
#endif
);

// Simple state transition (no hierarchy traversal)
// Calls: source.Exit -> target.Entry
state_machine_result_t switch_state(
    state_machine_t* const pState_Machine,
    const state_t* const pTarget_State
);

// Hierarchical state transition (HSM only)
// Traverses hierarchy, calling appropriate Exit/Entry actions
state_machine_result_t traverse_state(
    state_machine_t* const pState_Machine,
    const state_t* pTarget_State
);
```

## Configuration (hsm_config.h)

```c
// Enable hierarchical state machine support
#define HIERARCHICAL_STATES 1      // 1=HSM+FSM, 0=FSM only (default: 1)

// Enable state machine logging
#define STATE_MACHINE_LOGGER 0     // 1=enabled, 0=disabled (default: 0)

// Use variable length arrays in traverse_state
#define HSM_USE_VARIABLE_LENGTH_ARRAY 1  // 1=VLA, 0=fixed array (default: 1)

// If VLA disabled, define maximum hierarchy depth
#define MAX_HIERARCHICAL_LEVEL 5   // Required when VLA=0
```

## Usage Example

### Basic FSM

```c
#include <stdio.h>
#include <stdint.h>
#include "hsm_config.h"
#include "hsm.h"

// Events
enum { EV_NONE, EV_START, EV_STOP, EV_TIMEOUT };

// Forward declare states
extern const state_t idleState;
extern const state_t runningState;

// Derived state machine with custom data
typedef struct {
    state_machine_t machine;  // Must be first member
    int counter;
} MyStateMachine;

// State handlers
state_machine_result_t idle_handler(state_machine_t* const sm) {
    MyStateMachine* my = (MyStateMachine*)sm;

    switch (sm->Event) {
        case EV_START:
            printf("Starting...\n");
            return switch_state(sm, &runningState);
        default:
            return EVENT_UN_HANDLED;
    }
}

state_machine_result_t running_handler(state_machine_t* const sm) {
    MyStateMachine* my = (MyStateMachine*)sm;

    switch (sm->Event) {
        case EV_STOP:
            printf("Stopping...\n");
            return switch_state(sm, &idleState);
        case EV_TIMEOUT:
            my->counter++;
            printf("Tick %d\n", my->counter);
            return EVENT_HANDLED;
        default:
            return EVENT_UN_HANDLED;
    }
}

// Entry/exit actions
state_machine_result_t idle_entry(state_machine_t* const sm) {
    printf("[Enter IDLE]\n");
    return EVENT_HANDLED;
}

state_machine_result_t running_entry(state_machine_t* const sm) {
    printf("[Enter RUNNING]\n");
    return EVENT_HANDLED;
}

// State definitions
const state_t idleState = {
    .Handler = idle_handler,
    .Entry = idle_entry,
    .Exit = NULL,
};

const state_t runningState = {
    .Handler = running_handler,
    .Entry = running_entry,
    .Exit = NULL,
};

int main(void) {
    MyStateMachine my_sm = {
        .machine = { .Event = 0, .State = &idleState },
        .counter = 0
    };

    state_machine_t* sm_array[] = { &my_sm.machine };

    // Initialize
    my_sm.machine.State->Entry(&my_sm.machine);

    // Send events
    my_sm.machine.Event = EV_START;
    dispatch_event(sm_array, 1);

    my_sm.machine.Event = EV_TIMEOUT;
    dispatch_event(sm_array, 1);

    my_sm.machine.Event = EV_TIMEOUT;
    dispatch_event(sm_array, 1);

    my_sm.machine.Event = EV_STOP;
    dispatch_event(sm_array, 1);

    return 0;
}
```

### Hierarchical State Machine

```c
#include "hsm_config.h"
#include "hsm.h"

// Hierarchical states for a media player
// Hierarchy:
//   operating (level 0)
//     stopped (level 1)
//     playing (level 1)
//       normal (level 2)
//       fast_forward (level 2)

extern const state_t operatingState;
extern const state_t stoppedState;
extern const state_t playingState;
extern const state_t normalState;
extern const state_t fastForwardState;

// Parent state - handles common events for all children
state_machine_result_t operating_handler(state_machine_t* const sm) {
    if (sm->Event == EV_POWER_OFF) {
        // Common handling for all child states
        return switch_state(sm, &offState);
    }
    return EVENT_UN_HANDLED;
}

// Child states
const state_t operatingState = {
    .Handler = operating_handler,
    .Entry = NULL,
    .Exit = NULL,
    .Parent = NULL,           // Top-level state
    .Node = &stoppedState,    // Default child
    .Level = 0,
};

const state_t stoppedState = {
    .Handler = stopped_handler,
    .Entry = stopped_entry,
    .Exit = stopped_exit,
    .Parent = &operatingState,
    .Node = NULL,
    .Level = 1,
};

const state_t playingState = {
    .Handler = playing_handler,
    .Entry = playing_entry,
    .Exit = playing_exit,
    .Parent = &operatingState,
    .Node = &normalState,     // Default to normal playback
    .Level = 1,
};

const state_t normalState = {
    .Handler = normal_handler,
    .Entry = NULL,
    .Exit = NULL,
    .Parent = &playingState,
    .Node = NULL,
    .Level = 2,
};

// Use traverse_state for cross-hierarchy transitions
state_machine_result_t stopped_handler(state_machine_t* const sm) {
    if (sm->Event == EV_PLAY) {
        // Transition from stopped (level 1) to normal (level 2)
        // traverse_state handles: stopped.Exit -> playing.Entry -> normal.Entry
        return traverse_state(sm, &normalState);
    }
    return EVENT_UN_HANDLED;
}
```

## Transition Functions

### switch_state() - Simple Transition

Use for:
- FSM transitions (non-hierarchical)
- HSM transitions between siblings with same parent

```c
// Calls: current.Exit -> target.Entry
return switch_state(sm, &targetState);
```

### traverse_state() - Hierarchical Transition

Use for:
- HSM transitions between states at different hierarchy levels
- Cross-branch transitions

```c
// Traverses hierarchy, calling appropriate Exit/Entry actions
return traverse_state(sm, &targetState);
```

## Event Priority

When using multiple state machines, array index determines priority:

```c
state_machine_t* machines[] = {
    &high_priority_sm,    // Index 0 - highest priority
    &medium_priority_sm,  // Index 1
    &low_priority_sm      // Index 2 - lowest priority
};

// Events in machines[0] are always processed first
dispatch_event(machines, 3);
```

## Build Instructions

Requires header, source, and config files:

```bash
tcc myprogram.c hsm.c -o myprogram
```

## Files Required

- `hsm.h` - API and structure declarations
- `hsm.c` - Implementation
- `hsm_config.h` - Compile-time configuration

## VOS/TCC Compatibility Notes

- **TCC Compatible**: Yes - uses standard C99 features
- **VLA Support**: TCC supports VLAs; if issues arise, set `HSM_USE_VARIABLE_LENGTH_ARRAY` to 0
- **Dependencies**: `<stdint.h>`, `<stdbool.h>`, `<stdio.h>` (for logging)
- **Memory**: All state definitions can be const/ROM
- **Build**: `tcc myfile.c hsm.c -o myfile`

## Implementation Notes

- State machine struct must be first member of derived types for casting
- Event value 0 means "no pending event"
- `dispatch_event()` restarts from index 0 after handling any event
- Handler returning `TRIGGERED_TO_SELF` indicates it posted a new event
- `EVENT_UN_HANDLED` at top-level state is a fatal error
- For HSM, ensure `Level` values are correctly set (0 = top level)
- `traverse_state()` finds common ancestor and traverses via exit/entry actions
