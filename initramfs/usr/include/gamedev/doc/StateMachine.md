# StateMachine.h - Event-Driven State Machine

A compact C finite state machine (FSM) implementation with support for guards, entry/exit actions, and event-driven transitions.

## Overview

`StateMachine.h` is a C language state machine implementation designed for embedded and PC-based systems. It uses a run-to-completion execution model with support for both simple state machines and extended versions with guard/entry/exit functionality. State machine instances are defined using macros that generate the necessary boilerplate code.

## Original Source

**Repository**: https://github.com/endurodave/C_StateMachine

**Author**: David Lafreniere (endurodave)

**License**: MIT License

## Features

- **Macro-based definition** - Macros simplify state machine creation
- **Transition tables** - Compile-time transition validation
- **External/internal events** - Events from outside or self-generated
- **Guard conditions** - Conditional state transitions (extended version)
- **Entry/exit actions** - State lifecycle callbacks (extended version)
- **Event data** - Pass typed data with events
- **Thread safety** - Software locks for multithread access
- **Compile-time validation** - Static assertions for transition table sizes
- **Multiple instances** - Support multiple instances of the same state machine type

## API Reference

### Types

```c
// State machine constant data (read-only, can be in ROM)
typedef struct {
    const CHAR* name;
    const BYTE maxStates;
    const struct SM_StateStruct* stateMap;      // Simple version
    const struct SM_StateStructEx* stateMapEx;  // Extended version
} SM_StateMachineConst;

// State machine instance data (runtime state)
typedef struct {
    const CHAR* name;
    void* pInstance;           // Pointer to derived state machine object
    BYTE newState;
    BYTE currentState;
    BOOL eventGenerated;
    void* pEventData;
} SM_StateMachine;

// Simple state entry
typedef struct SM_StateStruct {
    SM_StateFunc pStateFunc;
} SM_StateStruct;

// Extended state entry (with guard/entry/exit)
typedef struct SM_StateStructEx {
    SM_StateFunc pStateFunc;
    SM_GuardFunc pGuardFunc;
    SM_EntryFunc pEntryFunc;
    SM_ExitFunc pExitFunc;
} SM_StateStructEx;

// Function signatures
typedef void (*SM_StateFunc)(SM_StateMachine* self, void* pEventData);
typedef BOOL (*SM_GuardFunc)(SM_StateMachine* self, void* pEventData);
typedef void (*SM_EntryFunc)(SM_StateMachine* self, void* pEventData);
typedef void (*SM_ExitFunc)(SM_StateMachine* self);
```

### Special Values

```c
enum {
    EVENT_IGNORED = 0xFE,   // Event should be ignored in this state
    CANNOT_HAPPEN = 0xFF    // Event cannot occur in this state (fault)
};

typedef void NoEventData;   // Use when event has no data
```

### Declaration/Definition Macros

```c
// Declare state machine externally
SM_DECLARE(smName)

// Define state machine instance
SM_DEFINE(smName, instancePtr)

// Declare/define event functions
EVENT_DECLARE(eventFunc, EventDataType)
EVENT_DEFINE(eventFunc, EventDataType)

// Declare/define state functions
STATE_DECLARE(stateFunc, EventDataType)
STATE_DEFINE(stateFunc, EventDataType)

// Extended: Declare/define guards, entries, exits
GUARD_DECLARE(guardFunc, EventDataType)
GUARD_DEFINE(guardFunc, EventDataType)

ENTRY_DECLARE(entryFunc, EventDataType)
ENTRY_DEFINE(entryFunc, EventDataType)

EXIT_DECLARE(exitFunc)
EXIT_DEFINE(exitFunc)
```

### State Map Macros (Simple Version)

```c
BEGIN_STATE_MAP(smName)
    STATE_MAP_ENTRY(ST_StateOne)
    STATE_MAP_ENTRY(ST_StateTwo)
    // ...
END_STATE_MAP(smName)
```

### State Map Macros (Extended Version)

```c
BEGIN_STATE_MAP_EX(smName)
    STATE_MAP_ENTRY_EX(ST_StateOne)  // No guard/entry/exit
    STATE_MAP_ENTRY_ALL_EX(ST_StateTwo, GD_Guard, EN_Entry, EX_Exit)
    // ...
END_STATE_MAP_EX(smName)
```

### Transition Map Macros

```c
BEGIN_TRANSITION_MAP
    TRANSITION_MAP_ENTRY(ST_NewState)     // From state 0
    TRANSITION_MAP_ENTRY(EVENT_IGNORED)   // From state 1
    TRANSITION_MAP_ENTRY(CANNOT_HAPPEN)   // From state 2
END_TRANSITION_MAP(smName, EventDataType)
```

### Public Interface Macros

```c
// Trigger external event
SM_Event(smName, eventFunc, pEventData)

// Get state machine property
SM_Get(smName, getFunc)
```

### Internal Macros (for state functions)

```c
// Generate internal event (self-transition)
SM_InternalEvent(newState, pEventData)

// Get instance pointer in state function
SM_GetInstance(InstanceType)
```

## Usage Example

```c
#include <stdio.h>
#include <string.h>
#include "DataTypes.h"
#include "Fault.h"
#include "StateMachine.h"

// States
enum DoorStates {
    ST_CLOSED,
    ST_OPENING,
    ST_OPEN,
    ST_CLOSING,
    ST_MAX_STATES
};

// Instance data
typedef struct {
    int motorSpeed;
    int position;
} DoorInstance;

static DoorInstance doorInstance = {0, 0};

// Event data
typedef struct {
    int targetPosition;
} OpenEventData;

// State machine declaration
SM_DECLARE(Door)

// Event declarations
EVENT_DECLARE(Door_Open, OpenEventData)
EVENT_DECLARE(Door_Close, NoEventData)
EVENT_DECLARE(Door_Stop, NoEventData)

// State declarations
STATE_DECLARE(Closed, OpenEventData)
STATE_DECLARE(Opening, NoEventData)
STATE_DECLARE(Open, NoEventData)
STATE_DECLARE(Closing, NoEventData)

// State machine definition
SM_DEFINE(Door, &doorInstance)

// State implementations
STATE_DEFINE(Closed, OpenEventData) {
    DoorInstance* inst = SM_GetInstance(DoorInstance);
    printf("Door closed at position %d\n", inst->position);
}

STATE_DEFINE(Opening, NoEventData) {
    DoorInstance* inst = SM_GetInstance(DoorInstance);
    inst->motorSpeed = 100;
    printf("Door opening, motor speed: %d\n", inst->motorSpeed);
}

STATE_DEFINE(Open, NoEventData) {
    DoorInstance* inst = SM_GetInstance(DoorInstance);
    inst->motorSpeed = 0;
    inst->position = 100;
    printf("Door fully open at position %d\n", inst->position);
}

STATE_DEFINE(Closing, NoEventData) {
    DoorInstance* inst = SM_GetInstance(DoorInstance);
    inst->motorSpeed = -100;
    printf("Door closing, motor speed: %d\n", inst->motorSpeed);
}

// Event implementations
EVENT_DEFINE(Door_Open, OpenEventData) {
    BEGIN_TRANSITION_MAP
        TRANSITION_MAP_ENTRY(ST_OPENING)   // ST_CLOSED -> ST_OPENING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_OPENING (already opening)
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_OPEN (already open)
        TRANSITION_MAP_ENTRY(ST_OPENING)   // ST_CLOSING -> ST_OPENING
    END_TRANSITION_MAP(Door, OpenEventData)
}

EVENT_DEFINE(Door_Close, NoEventData) {
    BEGIN_TRANSITION_MAP
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_CLOSED (already closed)
        TRANSITION_MAP_ENTRY(ST_CLOSING)   // ST_OPENING -> ST_CLOSING
        TRANSITION_MAP_ENTRY(ST_CLOSING)   // ST_OPEN -> ST_CLOSING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_CLOSING (already closing)
    END_TRANSITION_MAP(Door, NoEventData)
}

EVENT_DEFINE(Door_Stop, NoEventData) {
    BEGIN_TRANSITION_MAP
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_CLOSED
        TRANSITION_MAP_ENTRY(ST_OPEN)      // ST_OPENING -> ST_OPEN
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_OPEN
        TRANSITION_MAP_ENTRY(ST_CLOSED)    // ST_CLOSING -> ST_CLOSED
    END_TRANSITION_MAP(Door, NoEventData)
}

// State map
BEGIN_STATE_MAP(Door)
    STATE_MAP_ENTRY(ST_Closed)
    STATE_MAP_ENTRY(ST_Opening)
    STATE_MAP_ENTRY(ST_Open)
    STATE_MAP_ENTRY(ST_Closing)
END_STATE_MAP(Door)

int main(void) {
    printf("=== Door State Machine ===\n\n");

    // Allocate event data
    OpenEventData* openData = (OpenEventData*)SM_XAlloc(sizeof(OpenEventData));
    openData->targetPosition = 100;

    // Trigger events
    SM_Event(Door, Door_Open, openData);   // CLOSED -> OPENING
    SM_Event(Door, Door_Stop, NULL);       // OPENING -> OPEN
    SM_Event(Door, Door_Close, NULL);      // OPEN -> CLOSING
    SM_Event(Door, Door_Stop, NULL);       // CLOSING -> CLOSED

    return 0;
}
```

## Memory Allocation

Event data must be dynamically allocated using `SM_XAlloc()` and is automatically freed by the state machine engine after processing:

```c
// Allocate event data
MyEventData* data = (MyEventData*)SM_XAlloc(sizeof(MyEventData));
data->value = 42;

// Pass to event (ownership transferred)
SM_Event(MySM, MyEvent, data);
// data is automatically freed after state function executes
```

Configuration for allocator:

```c
#define USE_SM_ALLOCATOR        // Use fixed-block allocator
// or
#undef USE_SM_ALLOCATOR         // Use malloc/free
```

## Dependencies

This library requires two stub headers for VOS compatibility:

**DataTypes.h** - Type definitions:
```c
typedef uint8_t BYTE;
typedef int8_t CHAR;
typedef bool BOOL;
#define TRUE 1
#define FALSE 0
```

**Fault.h** - Error handling:
```c
#define ASSERT_TRUE(condition) ...
#define FaultHandler(msg) ...
```

## Build Instructions

Requires both header and source file plus dependencies:

```bash
tcc myprogram.c StateMachine.c -o myprogram
```

## VOS/TCC Compatibility Notes

- **TCC Compatible**: Yes - with provided stub headers
- **Files Required**: `StateMachine.h`, `StateMachine.c`, `DataTypes.h`, `Fault.h`
- **Dependencies**: `<stdint.h>`, `<stdbool.h>`, `<stdlib.h>` or custom allocator
- **Memory**: Dynamic allocation for event data
- **Build**: `tcc myfile.c StateMachine.c -o myfile`

## Implementation Notes

- State functions receive `SM_StateMachine*` and event data pointer
- `SM_GetInstance()` retrieves the typed instance from `self->pInstance`
- Internal events execute immediately within the same thread context
- External events validate transitions against the transition map
- `CANNOT_HAPPEN` triggers a fault handler (use for impossible transitions)
- `EVENT_IGNORED` silently discards the event (no state change)
- Event data is automatically freed after state function completes
- Extended state machine (EX) supports guard/entry/exit for complex scenarios
