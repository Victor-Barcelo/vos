# sm.h - Simple State Machine

A single-header library for creating statically allocated state machines in C.

## Overview

`sm.h` is a lightweight, feature-rich state machine library that supports states with entry/do/exit actions, transitions with guards, triggers, and effects. All state machine structures (except context) are statically allocated via macros, making it suitable for embedded systems.

## Original Source

**Repository**: https://github.com/Stemt/sm.h

**Author**: Alaric de Ruiter (Stemt)

**License**: MIT License

## Features

- **Single-header implementation** - Include one file, define `SM_IMPLEMENTATION` once
- **Static allocation** - All structures except `SM_Context` are statically allocated
- **State actions** - Enter, do, and exit actions per state
- **Transition flexibility** - Guards, triggers, and effects on transitions
- **Multiple instances** - Run multiple state machine instances via separate contexts
- **Tracing support** - Define `SM_TRACE` to log transitions to stderr
- **Custom prefix** - Override symbol prefix with `SM_PREFIX`
- **Configurable assert** - Override `SM_ASSERT` for custom error handling

## API Reference

### Types

```c
typedef void (*SM_ActionCallback)(void* user_context);
typedef bool (*SM_GuardCallback)(void* user_context);
typedef bool (*SM_TriggerCallback)(void* user_context, void* event);

typedef struct {
    SM_ActionCallback enter_action;   // Called on state entry
    SM_ActionCallback do_action;      // Called during SM_step() if no transition
    SM_ActionCallback exit_action;    // Called on state exit
    const char* trace_name;           // Name for tracing output
    void* transition;                 // Internal: linked list of transitions
    bool init;                        // Internal: initialization flag
} SM_State;

typedef struct {
    SM_TriggerCallback trigger;       // Event-based trigger condition
    SM_GuardCallback guard;           // Guard condition
    SM_ActionCallback effect;         // Action executed during transition
    SM_State* source;                 // Source state
    SM_State* target;                 // Target state
    void* next_transition;            // Internal: linked list
    bool init;                        // Internal: initialization flag
} SM_Transition;

typedef struct {
    void* user_context;               // User data passed to callbacks
    SM_State* current_state;          // Current active state
    bool halted;                      // True if final state reached
} SM_Context;

typedef struct {
    size_t transition_count;
    SM_Transition** transitions;
    SM_Transition* initial_transition;
    bool init;
} SM;
```

### Special States

```c
#define SM_INITIAL_STATE NULL   // Starting pseudo-state
#define SM_FINAL_STATE NULL     // Ending pseudo-state (halts machine)
```

### State Machine Definition

```c
// Define a state machine (can be in global or local scope)
SM_def(sm);
```

### State Creation and Configuration

```c
// Create a state (must be in local scope)
SM_State_create(state_name);

// Initialize a state manually
void SM_State_init(SM_State* self);

// Set state callbacks
void SM_State_set_enter_action(SM_State* self, SM_ActionCallback action);
void SM_State_set_do_action(SM_State* self, SM_ActionCallback action);
void SM_State_set_exit_action(SM_State* self, SM_ActionCallback action);
```

### Transition Creation and Configuration

```c
// Create and add a transition
SM_Transition_create(sm, transition_name, source_state, target_state);

// Initialize a transition manually
void SM_Transition_init(SM_Transition* self, SM_State* source, SM_State* target);

// Set transition callbacks
void SM_Transition_set_trigger(SM_Transition* self, SM_TriggerCallback trigger);
void SM_Transition_set_guard(SM_Transition* self, SM_GuardCallback guard);
void SM_Transition_set_effect(SM_Transition* self, SM_ActionCallback effect);

// Add transition to state machine
void SM_add_transition(SM* self, SM_Transition* transition);
```

### Context Management

```c
// Initialize context with user data
void SM_Context_init(SM_Context* self, void* user_context);

// Reset context (keeps user_context, resets state)
void SM_Context_reset(SM_Context* self);

// Check if machine has halted (reached SM_FINAL_STATE)
bool SM_Context_is_halted(SM_Context* self);
```

### Execution

```c
// Perform one step: check guards, trigger transitions, or execute do_action
// Returns true if action/transition performed, false if halted
bool SM_step(SM* self, SM_Context* context);

// Notify state machine of an event (for trigger-based transitions)
// Returns true if event was handled (caused transition)
bool SM_notify(SM* self, SM_Context* context, void* event);

// Run until halted (calls SM_step() in a loop)
void SM_run(SM* self, SM_Context* context);
```

## Usage Example

```c
#include <stdio.h>
#include <stdbool.h>

#define SM_IMPLEMENTATION
#include "sm.h"

// Game context
typedef struct {
    int score;
    bool game_over;
} GameContext;

// State callbacks
void enter_playing(void* ctx) {
    printf("Game started!\n");
}

void do_playing(void* ctx) {
    GameContext* game = (GameContext*)ctx;
    game->score += 10;
    printf("Playing... Score: %d\n", game->score);
}

void enter_gameover(void* ctx) {
    GameContext* game = (GameContext*)ctx;
    printf("Game Over! Final score: %d\n", game->score);
}

// Guard: check if game should end
bool guard_game_over(void* ctx) {
    GameContext* game = (GameContext*)ctx;
    return game->score >= 50;
}

int main(void) {
    SM_def(game_sm);

    // Create states
    SM_State_create(playing);
    SM_State_set_enter_action(playing, enter_playing);
    SM_State_set_do_action(playing, do_playing);

    SM_State_create(gameover);
    SM_State_set_enter_action(gameover, enter_gameover);

    // Create transitions
    SM_Transition_create(game_sm, start, SM_INITIAL_STATE, playing);

    SM_Transition_create(game_sm, end_game, playing, gameover);
    SM_Transition_set_guard(end_game, guard_game_over);

    SM_Transition_create(game_sm, finish, gameover, SM_FINAL_STATE);

    // Initialize and run
    GameContext ctx = {0, false};
    SM_Context sm_ctx;
    SM_Context_init(&sm_ctx, &ctx);

    SM_run(game_sm, &sm_ctx);

    return 0;
}
```

## Transition Triggering Modes

### 1. Guard Only (Automatic)
Transition triggers during `SM_step()` when guard returns true:

```c
SM_Transition_set_guard(trans, my_guard);  // No trigger set
// Transition fires automatically when my_guard() returns true
```

### 2. Trigger Only (Event-based)
Transition triggers during `SM_notify()` when trigger returns true:

```c
SM_Transition_set_trigger(trans, my_trigger);
// Transition fires when SM_notify() is called and my_trigger() returns true
```

### 3. Guard + Trigger (Conditional Event)
Guard is checked first, then trigger during `SM_notify()`:

```c
SM_Transition_set_guard(trans, my_guard);
SM_Transition_set_trigger(trans, my_trigger);
// Transition fires when guard allows AND trigger accepts event
```

### 4. No Guard or Trigger (Immediate)
Transition fires immediately during `SM_step()`:

```c
// No guard or trigger set - transitions unconditionally
```

## Configuration Macros

| Macro | Default | Description |
|-------|---------|-------------|
| `SM_IMPLEMENTATION` | (undefined) | Define in ONE source file to include implementation |
| `SM_TRACE` | (undefined) | Define to enable transition logging to stderr |
| `SM_TRACE_LOG_FMT` | `fprintf(stderr, ...)` | Custom trace log format function |
| `SM_PREFIX` | `SM_` | Prefix for generated symbol names |
| `SM_ASSERT` | `assert()` | Custom assertion macro |

## VOS/TCC Compatibility Notes

- **TCC Compatible**: Yes - uses standard C99 features
- **Header-only**: Include with `#define SM_IMPLEMENTATION` in one .c file
- **Dependencies**: `<stdbool.h>`, `<stddef.h>`, `<assert.h>`, `<stdio.h>` (if tracing)
- **Memory**: Statically allocated (no malloc/free required)
- **Build**: `tcc -DSM_IMPLEMENTATION myfile.c -o myfile`

## Implementation Notes

- State machines must have at least one transition from `SM_INITIAL_STATE`
- Transitions form a linked list per source state
- `SM_step()` checks guard-only transitions first, then unconditional transitions
- `SM_notify()` is for event-driven transitions with triggers
- Reaching `SM_FINAL_STATE` sets `context.halted = true`
