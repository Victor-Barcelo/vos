# stately.h - Minimal Finite State Machine

A single-header, minimal finite-state machine library for C with table-driven state transitions.

## Overview

`stately.h` is a lightweight FSM library that uses a transition table approach. It maps inputs to states using a polymorphic mapping function, making it flexible for various input types while keeping the implementation simple and efficient.

## Original Source

**Repository**: https://github.com/jnguyen1098/stately

**Author**: jnguyen1098

**License**: ISC License

## Features

- **Single-header implementation** - Just include `stately.h`
- **Table-driven transitions** - Uses 2D state table for O(1) lookups
- **Polymorphic input mapping** - Custom `map()` function for any input type
- **Automatic trap states** - Invalid inputs handled via C's zero-initialization
- **Enum-friendly** - States are integers, work seamlessly with enums
- **Pure functional queries** - `SUPPOSE_STATE` macro for hypothetical transitions
- **Configurable limits** - `MAX_STATES` and `MAX_ALPHABET_SIZE` macros

## API Reference

### Data Structure

```c
struct state_machine {
    int curr_state;                                    // Current state
    int (*map)(const void *);                          // Input-to-index mapper
    int state_table[MAX_STATES][MAX_ALPHABET_SIZE + 1]; // Transition table
};
```

### Configuration Macros

```c
#ifndef MAX_ALPHABET_SIZE
#define MAX_ALPHABET_SIZE 256    // Maximum input alphabet size
#endif

#ifndef MAX_STATES
#define MAX_STATES 128           // Maximum number of states
#endif
```

### Core Macros

```c
// Set current state directly
SET_STATE(machine, state)

// Get current state
GET_STATE(machine)

// Get next state for input without changing current state (pure/functional)
SUPPOSE_STATE(machine, state, input)

// Transition to next state based on input (modifies current state)
GET_NEXT_STATE(machine, input)
```

### Map Function Signature

```c
int map_function(const void *input);
```

The `map()` function converts an arbitrary input to a state table index. Similar to `qsort()`'s comparator pattern, it takes a `const void*` and returns an `int`.

## Usage Example

```c
#include <stdio.h>
#include "stately.h"

// States
enum PlayerState {
    IDLE,
    WALKING,
    RUNNING,
    JUMPING,
    STATE_COUNT
};

// Inputs
enum Input {
    INPUT_NONE,
    INPUT_MOVE,
    INPUT_RUN,
    INPUT_JUMP,
    INPUT_STOP,
    INPUT_COUNT
};

// State names for display
const char* state_names[] = {"IDLE", "WALKING", "RUNNING", "JUMPING"};

// Map function: convert input pointer to index
int input_mapper(const void *input) {
    return *(const int*)input;
}

int main(void) {
    // Initialize state machine
    struct state_machine player = {
        .curr_state = IDLE,
        .map = input_mapper,
        .state_table = {0}  // All zeros = stay in current state (trap)
    };

    // Define transition table
    // state_table[current_state][input] = next_state

    // From IDLE
    player.state_table[IDLE][INPUT_MOVE] = WALKING;
    player.state_table[IDLE][INPUT_JUMP] = JUMPING;

    // From WALKING
    player.state_table[WALKING][INPUT_STOP] = IDLE;
    player.state_table[WALKING][INPUT_RUN] = RUNNING;
    player.state_table[WALKING][INPUT_JUMP] = JUMPING;

    // From RUNNING
    player.state_table[RUNNING][INPUT_STOP] = IDLE;
    player.state_table[RUNNING][INPUT_MOVE] = WALKING;
    player.state_table[RUNNING][INPUT_JUMP] = JUMPING;

    // From JUMPING (lands back to IDLE)
    player.state_table[JUMPING][INPUT_STOP] = IDLE;

    // Test transitions
    printf("Initial state: %s\n\n", state_names[GET_STATE(player)]);

    int inputs[] = {INPUT_MOVE, INPUT_RUN, INPUT_JUMP, INPUT_STOP};
    const char* input_names[] = {"", "MOVE", "RUN", "JUMP", "STOP"};

    for (int i = 0; i < 4; i++) {
        int input = inputs[i];
        int old_state = GET_STATE(player);

        // Query hypothetical next state without changing
        int hypothetical = SUPPOSE_STATE(player, old_state, &input);
        printf("SUPPOSE: %s + %s -> %s\n",
               state_names[old_state], input_names[input], state_names[hypothetical]);

        // Actually transition
        int new_state = GET_NEXT_STATE(player, &input);
        printf("ACTUAL:  %s -> %s\n\n", state_names[old_state], state_names[new_state]);
    }

    return 0;
}
```

## Character-Based Input Example

For character-based FSMs (like lexers), the map function can directly cast:

```c
// Character input mapper
int char_mapper(const void *input) {
    return (int)*(const char*)input;
}

// Usage with character input
struct state_machine lexer = {
    .curr_state = 0,
    .map = char_mapper,
    .state_table = {0}
};

// Define transitions for characters
lexer.state_table[0]['a'] = 1;  // On 'a', go to state 1
lexer.state_table[0]['b'] = 2;  // On 'b', go to state 2
// Invalid characters (not defined) automatically stay in current state

char c = 'a';
int next = GET_NEXT_STATE(lexer, &c);
```

## Trap States and Error Handling

Due to C's zero-initialization of static/global arrays, undefined transitions automatically go to state 0 (or stay at current state if you initialize transitions to return to current state). This provides automatic trap state handling:

```c
struct state_machine sm = {0};  // All transitions default to 0

// Only define valid transitions
sm.state_table[STATE_A][INPUT_X] = STATE_B;

// Undefined: sm.state_table[STATE_A][INPUT_Y] remains 0
// Acts as rejection/trap state
```

## Advanced: Conditional Transitions

For guard conditions, implement logic in your map function or check after transition:

```c
typedef struct {
    int input_type;
    int parameter;
} ComplexInput;

int complex_mapper(const void *input) {
    const ComplexInput *ci = (const ComplexInput*)input;

    // Map based on input type and conditions
    if (ci->input_type == ATTACK && ci->parameter > 50) {
        return INPUT_STRONG_ATTACK;
    }
    return ci->input_type;
}
```

## VOS/TCC Compatibility Notes

- **TCC Compatible**: Yes - uses only standard C features
- **Header-only**: Just include `stately.h`, no implementation define needed
- **Dependencies**: None (self-contained)
- **Memory**: Static allocation via fixed-size arrays
- **Build**: `tcc myfile.c -o myfile`

## Configuration

Override limits before including the header:

```c
#define MAX_ALPHABET_SIZE 32   // Reduce for smaller memory footprint
#define MAX_STATES 16
#include "stately.h"
```

## Implementation Notes

- State table is `MAX_STATES x (MAX_ALPHABET_SIZE + 1)` integers
- Memory usage: `MAX_STATES * (MAX_ALPHABET_SIZE + 1) * sizeof(int)` bytes per machine
- Default configuration: 128 * 257 * 4 = ~131 KB per state machine
- For embedded systems, reduce `MAX_STATES` and `MAX_ALPHABET_SIZE`
- `SUPPOSE_STATE` is purely functional - useful for AI lookahead or validation
- `GET_NEXT_STATE` both queries and updates `curr_state`
