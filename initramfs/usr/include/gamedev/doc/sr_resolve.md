# sr_resolve - Swept AABB Collision Resolver

## Description

sr_resolve is a simple single-header C/C++ library for Axis-Aligned Bounding Box (AABB) collision detection and resolution. It implements swept collision detection to prevent tunneling (objects passing through each other at high speeds) and provides a "move and slide" function commonly used for character movement in games.

## Original Source

- **Repository**: [https://github.com/siddharthroy12/sr_resolve](https://github.com/siddharthroy12/sr_resolve)
- **Author**: Siddharth Roy
- **Version**: 1.0 (2021)
- **Example Project**: [https://github.com/siddharthroy12/sr_resolve_example](https://github.com/siddharthroy12/sr_resolve_example)

## License

The header states "Siddharth Roy 2021" but does not include an explicit license declaration. Check the repository for current licensing information.

## Features

- Single header file implementation
- Axis-Aligned Bounding Box (AABB) collision detection
- Ray vs Rectangle intersection testing
- Swept/dynamic collision detection (prevents tunneling)
- Contact point and normal calculation
- Move-and-slide functionality for platformer/action games
- Broadphase optimization using swept bounding boxes
- Obstacle sorting by collision time for proper resolution order
- Float-precision calculations for game use

## API Reference

### Utility Macros

```c
#define SR_RESOLVE_MAX(x, y)  (x > y ? x : y)
#define SR_RESOLVE_MIN(x, y)  (x > y ? y : x)
```

### Structures

#### sr_rec
```c
typedef struct sr_rec {
    float x;        // X position
    float y;        // Y position
    float width;    // Rectangle width
    float height;   // Rectangle height
} sr_rec;
```

#### sr_vec2
```c
typedef struct sr_vec2 {
    float x;
    float y;
} sr_vec2;
```

#### sr_ray2
```c
typedef struct sr_ray2 {
    sr_vec2 position;   // Ray origin
    sr_vec2 direction;  // Ray direction
} sr_ray2;
```

#### sr_sort_pair
```c
typedef struct sr_sort_pair {
    int index;      // Obstacle index
    float time;     // Collision time
} sr_sort_pair;
```

### Vector Functions

```c
// Get vector length
static float sr_vec2_length(sr_vec2 v);

// Scale vector by scalar
static sr_vec2 sr_vec2_scale(sr_vec2 v, float scale);

// Divide two vectors component-wise
static sr_vec2 sr_vec2_divide(sr_vec2 v1, sr_vec2 v2);

// Multiply two vectors component-wise
static sr_vec2 sr_vec2_multiply(sr_vec2 v1, sr_vec2 v2);

// Normalize vector to unit length
static sr_vec2 sr_vec2_normalize(sr_vec2 v);

// Subtract two vectors
static sr_vec2 sr_vector2_sub(sr_vec2 v1, sr_vec2 v2);

// Add two vectors
static sr_vec2 sr_vector2_add(sr_vec2 v1, sr_vec2 v2);
```

### Collision Detection Functions

#### sr_check_rec_vs_rec_collision
```c
static bool sr_check_rec_vs_rec_collision(sr_rec rec1, sr_rec rec2);
```
Simple AABB overlap test between two rectangles.

**Parameters:**
- `rec1`: First rectangle
- `rec2`: Second rectangle

**Returns:** `true` if rectangles overlap, `false` otherwise

#### sr_check_ray_vs_rec_collision
```c
static bool sr_check_ray_vs_rec_collision(
    const sr_ray2 ray,
    const sr_rec target,
    sr_vec2 *contact_point,
    sr_vec2 *contact_normal,
    float *t_hit_near
);
```
Ray-rectangle intersection test with contact information.

**Parameters:**
- `ray`: Ray to test (position + direction)
- `target`: Target rectangle
- `contact_point`: Output - point where ray hits rectangle
- `contact_normal`: Output - surface normal at contact point
- `t_hit_near`: Output - parametric time of collision (0-1 range for direction length)

**Returns:** `true` if ray hits rectangle, `false` otherwise

#### sr_dynamic_rect_vs_rect
```c
static bool sr_dynamic_rect_vs_rect(
    const sr_rec in,
    const sr_rec target,
    sr_vec2 vel,
    sr_vec2 *contact_point,
    sr_vec2 *contact_normal,
    float *contact_time,
    float delta
);
```
Swept collision detection between a moving rectangle and a static rectangle.

**Parameters:**
- `in`: Moving rectangle
- `target`: Static obstacle rectangle
- `vel`: Velocity of moving rectangle
- `contact_point`: Output - collision contact point
- `contact_normal`: Output - collision surface normal
- `contact_time`: Output - time of collision (0-1 range within delta)
- `delta`: Time step (delta time)

**Returns:** `true` if collision will occur within the time step

### Main Game Function

#### sr_move_and_slide
```c
static void sr_move_and_slide(
    sr_rec *obstacles,
    int obstacles_length,
    sr_vec2 hitbox,
    sr_vec2 *vel,
    sr_vec2 *pos,
    float delta
);
```
The primary function for game character movement. Moves an entity while resolving collisions with multiple obstacles, implementing slide behavior along surfaces.

**Parameters:**
- `obstacles`: Array of obstacle rectangles
- `obstacles_length`: Number of obstacles in array
- `hitbox`: Size of the moving entity's hitbox (width, height)
- `vel`: Pointer to velocity (modified on collision)
- `pos`: Pointer to position (center of hitbox, modified after movement)
- `delta`: Time step (delta time)

**Behavior:**
1. Creates broadphase bounding box for the movement
2. Sorts obstacles by collision time
3. Iterates through obstacles in collision order
4. For each collision, moves entity to contact point and zeroes velocity component
5. Implements sliding along surfaces

## Usage Example

### Basic Rectangle Collision

```c
#include <stdbool.h>
#include "sr_resolve.h"

int main(void)
{
    // Player rectangle
    sr_rec player = { 100.0f, 100.0f, 32.0f, 32.0f };

    // Obstacle rectangle
    sr_rec obstacle = { 150.0f, 100.0f, 64.0f, 64.0f };

    // Simple overlap check
    if (sr_check_rec_vs_rec_collision(player, obstacle)) {
        // Handle collision
    }

    return 0;
}
```

### Move and Slide (Platformer Character)

```c
#include <stdbool.h>
#include "sr_resolve.h"

// Game state
sr_vec2 player_pos = { 100.0f, 100.0f };
sr_vec2 player_vel = { 0.0f, 0.0f };
sr_vec2 player_hitbox = { 32.0f, 48.0f };

// Level obstacles
sr_rec obstacles[3] = {
    { 0.0f, 400.0f, 800.0f, 32.0f },   // Floor
    { 200.0f, 300.0f, 100.0f, 32.0f }, // Platform
    { 0.0f, 0.0f, 32.0f, 400.0f }      // Wall
};

void update(float delta)
{
    // Apply gravity
    player_vel.y += 500.0f * delta;

    // Apply input
    if (key_left)  player_vel.x = -200.0f;
    if (key_right) player_vel.x = 200.0f;

    // Move and resolve collisions
    sr_move_and_slide(
        obstacles,
        3,
        player_hitbox,
        &player_vel,
        &player_pos,
        delta
    );

    // Apply friction when on ground
    if (player_vel.y == 0) {
        player_vel.x *= 0.9f;
    }
}

void draw(void)
{
    // Draw player centered on position
    draw_rectangle(
        player_pos.x - player_hitbox.x / 2,
        player_pos.y - player_hitbox.y / 2,
        player_hitbox.x,
        player_hitbox.y
    );
}
```

### Ray Casting Example

```c
#include <stdbool.h>
#include "sr_resolve.h"

void raycast_example(void)
{
    // Cast ray from player toward mouse
    sr_ray2 ray = {
        .position = { player_x, player_y },
        .direction = { mouse_x - player_x, mouse_y - player_y }
    };

    sr_rec wall = { 300.0f, 100.0f, 32.0f, 200.0f };

    sr_vec2 contact_point;
    sr_vec2 contact_normal;
    float hit_time;

    if (sr_check_ray_vs_rec_collision(ray, wall, &contact_point, &contact_normal, &hit_time)) {
        // Ray hits wall
        // contact_point = exact hit location
        // contact_normal = surface normal at hit (e.g., {-1, 0} for left side)
        // hit_time = 0-1 parametric distance along ray

        draw_line(ray.position.x, ray.position.y,
                  contact_point.x, contact_point.y);
    }
}
```

## VOS/TCC Compatibility Notes

### Dependencies
The library requires these standard headers:
- `math.h` - for `sqrtf()`, `isnanf()`, `fabs()`
- `stdio.h` - included but not actually used (can be removed)
- `stdbool.h` - required for `bool` type

### Important Considerations

1. **Position Convention**: `sr_move_and_slide` uses center-based positions for the hitbox, but obstacle rectangles use top-left corner positions.

2. **VLA Usage**: The function `sr_move_and_slide` uses a Variable Length Array:
   ```c
   sr_sort_pair times[obstacles_length];
   ```
   TCC supports VLAs, but if issues arise, replace with a fixed-size array or dynamic allocation.

3. **Float Operations**: Uses single-precision floats throughout, which is appropriate for game use.

### Compilation Notes
- Header-only library; just include `sr_resolve.h`
- Requires `<stdbool.h>` for `bool` type (include before or ensure VOS provides it)
- Link math library if required by your compiler (`-lm`)

### Memory Considerations
- No dynamic memory allocation (stack only)
- Sorting array is allocated on stack per frame
- Consider `obstacles_length` limit for stack safety

### Performance Tips
- Keep obstacle count reasonable (broadphase helps but sorting is O(n log n))
- Obstacles should be pre-filtered to those near the player for large levels
- The broadphase check reduces unnecessary detailed collision tests

### Coordinate System
- Assumes Y increases downward (typical screen coordinates)
- Gravity should be positive Y for downward pull
- Contact normals: `{0, 1}` = floor (hit from above), `{0, -1}` = ceiling

### Known Limitations
- Only handles axis-aligned boxes (no rotation)
- Single moving object vs static obstacles design
- No continuous collision detection between multiple moving objects
- Sliding may cause issues at corners with multiple simultaneous collisions
