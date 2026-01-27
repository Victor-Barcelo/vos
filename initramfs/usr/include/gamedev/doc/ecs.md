# ecs.h - Entity Component System

A lightweight, header-only Entity Component System (ECS) implementation in C.

## Overview

`ecs.h` is a simple yet functional ECS library designed for game development. It provides a data-oriented approach to organizing game entities and their behaviors, separating data (components) from logic (systems).

## Original Source

**Repository**: Custom/Unknown (header-only implementation)

**License**: Not explicitly stated in source file. Assumed permissive for inclusion in VOS gameResources.

## Features

- **Header-only implementation** - Define `ECS_IMPLEMENTATION` in one source file
- **Entity management** - Create, destroy, and validate entities with versioning
- **Component pools** - Efficient component storage with configurable pool sizes
- **System registration** - Register update and render systems separately
- **Component masks** - Check for multiple components efficiently
- **Custom allocators** - Override `ECS_MALLOC` and `ECS_FREE` macros
- **Optional logging** - Enable with `ECS_ENABLE_LOGGING`
- **Static linkage option** - Define `ECS_STATIC` for internal linkage

## API Reference

### Types

```c
typedef uint64_t EcsEnt;              // Entity identifier (index + version)
typedef uint32_t EcsComponentType;    // Component type identifier
typedef struct Ecs Ecs;               // Opaque ECS world handle

typedef enum {
    ECS_SYSTEM_UPDATE,
    ECS_SYSTEM_RENDER
} EcsSystemType;

typedef void (*ecs_system_func)(struct Ecs *ecs);
typedef void (*ecs_component_destroy)(void *data);
```

### Core Functions

#### World Management

```c
// Create an ECS world
Ecs* ecs_make(uint32_t max_entities, uint32_t component_count, uint32_t system_count);

// Destroy the ECS world and free all resources
void ecs_destroy(Ecs *ecs);
```

#### Component Registration

```c
// Register a component type with its pool configuration
void ecs_register_component(Ecs *ecs,
                            EcsComponentType component_type,
                            uint32_t count,           // Pool size
                            uint32_t size,            // Component size in bytes
                            ecs_component_destroy destroy_func);  // Optional destructor
```

#### System Registration

```c
// Register a system function
void ecs_register_system(Ecs *ecs, ecs_system_func func, EcsSystemType type);

// Run all systems of a specific type
void ecs_run_systems(Ecs *ecs, EcsSystemType type);

// Run a specific system by index
void ecs_run_system(Ecs *ecs, uint32_t system_index);
```

#### Entity Management

```c
// Create a new entity
EcsEnt ecs_ent_make(Ecs *ecs);

// Destroy an entity and all its components
void ecs_ent_destroy(Ecs *ecs, EcsEnt e);

// Check if an entity is valid (not destroyed/recycled)
bool ecs_ent_is_valid(Ecs *ecs, EcsEnt e);

// Get entity version (incremented on destruction)
uint32_t ecs_ent_get_version(Ecs *ecs, EcsEnt e);

// Debug: print entity information
void ecs_ent_print(Ecs *ecs, EcsEnt e);
```

#### Component Operations

```c
// Add a component to an entity
void ecs_ent_add_component(Ecs *ecs, EcsEnt e, EcsComponentType type, void *component_data);

// Remove a component from an entity
void ecs_ent_remove_component(Ecs *ecs, EcsEnt e, EcsComponentType type);

// Get a component from an entity (returns NULL if not present)
void* ecs_ent_get_component(Ecs *ecs, EcsEnt e, EcsComponentType type);

// Check if entity has a specific component
bool ecs_ent_has_component(Ecs *ecs, EcsEnt e, EcsComponentType component_type);

// Check if entity has all components in a mask
bool ecs_ent_has_mask(Ecs *ecs, EcsEnt e,
                      uint32_t component_type_count, EcsComponentType component_types[]);
```

#### Iteration Helpers

```c
// Get the count for iteration (max_index + 1)
uint32_t ecs_for_count(Ecs *ecs);

// Get entity at a specific index
EcsEnt ecs_get_ent(Ecs *ecs, uint32_t index);
```

### Macros

```c
// Helper macro for creating component masks
#define ECS_MASK(ctypes_count, ...) \
    ctypes_count, (EcsComponentType[]){__VA_ARGS__}
```

## Usage Example

```c
#include <stdio.h>
#include <string.h>

#define ECS_IMPLEMENTATION
#include "ecs.h"

// Define components
typedef struct { float x, y; } Position;
typedef struct { float vx, vy; } Velocity;

// Component type IDs
enum { COMP_POSITION = 0, COMP_VELOCITY, COMP_COUNT };

// Movement system
void movement_system(Ecs *ecs) {
    for (uint32_t i = 0; i < ecs_for_count(ecs); i++) {
        EcsEnt e = ecs_get_ent(ecs, i);
        if (!ecs_ent_is_valid(ecs, e)) continue;
        if (!ecs_ent_has_mask(ecs, e, ECS_MASK(2, COMP_POSITION, COMP_VELOCITY))) continue;

        Position *pos = ecs_ent_get_component(ecs, e, COMP_POSITION);
        Velocity *vel = ecs_ent_get_component(ecs, e, COMP_VELOCITY);
        pos->x += vel->vx;
        pos->y += vel->vy;
    }
}

int main(void) {
    // Create world: 100 entities, 2 component types, 1 system
    Ecs *world = ecs_make(100, COMP_COUNT, 1);

    // Register components
    ecs_register_component(world, COMP_POSITION, 100, sizeof(Position), NULL);
    ecs_register_component(world, COMP_VELOCITY, 100, sizeof(Velocity), NULL);

    // Register systems
    ecs_register_system(world, movement_system, ECS_SYSTEM_UPDATE);

    // Create entity with components
    EcsEnt player = ecs_ent_make(world);
    Position pos = {0, 0};
    Velocity vel = {1, 0.5f};
    ecs_ent_add_component(world, player, COMP_POSITION, &pos);
    ecs_ent_add_component(world, player, COMP_VELOCITY, &vel);

    // Game loop
    for (int frame = 0; frame < 60; frame++) {
        ecs_run_systems(world, ECS_SYSTEM_UPDATE);
    }

    // Cleanup
    ecs_destroy(world);
    return 0;
}
```

## Configuration Macros

| Macro | Default | Description |
|-------|---------|-------------|
| `ECS_IMPLEMENTATION` | (undefined) | Define in ONE source file to include implementation |
| `ECS_STATIC` | (undefined) | Define to make all functions static |
| `ECS_ENABLE_LOGGING` | (undefined) | Define to enable warning messages via printf |
| `ECS_MALLOC` | `malloc` | Custom memory allocation function |
| `ECS_FREE` | `free` | Custom memory deallocation function |

## VOS/TCC Compatibility Notes

- **TCC Compatible**: Yes - uses standard C99 features
- **Header-only**: Include with `#define ECS_IMPLEMENTATION` in one .c file
- **Dependencies**: `<stdint.h>`, `<stdbool.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`
- **Memory**: Uses dynamic allocation; can be customized via `ECS_MALLOC`/`ECS_FREE`
- **Build**: `tcc -DECS_IMPLEMENTATION myfile.c -o myfile`

## Implementation Notes

- Entity IDs combine a 32-bit index and 32-bit version in a 64-bit value
- Version increments on entity destruction, invalidating old references
- Component pools use a stack-based free list for O(1) allocation/deallocation
- Systems are stored in a simple array and executed in registration order
