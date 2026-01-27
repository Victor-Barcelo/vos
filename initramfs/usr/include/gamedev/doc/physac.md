# Physac - 2D Physics Library

## Description

Physac is a small 2D physics engine written in pure C. The engine uses a fixed time-step thread loop to simulate physics. A physics step contains the following phases:

1. Get collision information
2. Apply dynamics
3. Collision solving
4. Position correction

It uses a very simple struct for physics bodies with a position vector that can be used with any rendering API.

## Original Source

- **Repository**: [https://github.com/raysan5/physac](https://github.com/raysan5/physac)
- **Original Author**: Victor Fisac ([https://github.com/victorfisac/Physac](https://github.com/victorfisac/Physac))
- **Version**: 1.1

## License

**zlib/libpng License**

Copyright (c) 2016-2025 Victor Fisac

This software is provided "as-is", without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

## Features

- Small 2D physics engine written in pure C
- Fixed time-step thread loop for physics simulation
- Collision detection and resolution between various shapes
- Support for multiple body shapes:
  - Circles
  - Rectangles
  - Polygons (with configurable vertex count)
- Force and torque application
- Body shattering/explosion effects
- Configurable gravity simulation
- Friction modeling (static and dynamic)
- Rotation/angular velocity support
- Restitution (bounciness) coefficient
- Graphics-engine independent design
- Custom memory allocator support

## Configuration Macros

```c
// Include implementation in one source file
#define PHYSAC_IMPLEMENTATION

// Make functions static (private to the file)
#define PHYSAC_STATIC

// Disable threading (user must call PhysicsThread() manually)
#define PHYSAC_NO_THREADS

// Avoid raylib.h inclusion, use standalone mode
#define PHYSAC_STANDALONE

// Enable debug logging
#define PHYSAC_DEBUG

// Custom memory allocation
#define PHYSAC_MALLOC(size)   malloc(size)
#define PHYSAC_FREE(ptr)      free(ptr)
```

## Default Configuration Values

```c
#define PHYSAC_MAX_BODIES               64      // Maximum simultaneous bodies
#define PHYSAC_MAX_MANIFOLDS            4096    // Maximum collision contacts
#define PHYSAC_MAX_VERTICES             24      // Maximum polygon vertices
#define PHYSAC_CIRCLE_VERTICES          24      // Circle approximation vertices
#define PHYSAC_FIXED_TIME               1.0f/60.0f  // Physics time step
#define PHYSAC_COLLISION_ITERATIONS     20      // Solver iterations
#define PHYSAC_PENETRATION_ALLOWANCE    0.05f   // Overlap tolerance
#define PHYSAC_PENETRATION_CORRECTION   0.4f    // Correction strength
```

## API Reference

### Types

#### PhysicsShapeType
```c
typedef enum PhysicsShapeType {
    PHYSICS_CIRCLE,
    PHYSICS_POLYGON
} PhysicsShapeType;
```

#### Vector2 (in standalone mode)
```c
typedef struct Vector2 {
    float x;
    float y;
} Vector2;
```

#### Mat2
```c
typedef struct Mat2 {
    float m00, m01;
    float m10, m11;
} Mat2;
```

#### PolygonData
```c
typedef struct PolygonData {
    unsigned int vertexCount;
    Vector2 positions[PHYSAC_MAX_VERTICES];
    Vector2 normals[PHYSAC_MAX_VERTICES];
} PolygonData;
```

#### PhysicsShape
```c
typedef struct PhysicsShape {
    PhysicsShapeType type;      // Circle or polygon
    PhysicsBody body;           // Parent body reference
    float radius;               // Circle radius
    Mat2 transform;             // 2x2 rotation matrix
    PolygonData vertexData;     // Polygon vertices and normals
} PhysicsShape;
```

#### PhysicsBodyData
```c
typedef struct PhysicsBodyData {
    unsigned int id;            // Unique identifier
    bool enabled;               // Dynamics enabled state
    Vector2 position;           // Body position (pivot)
    Vector2 velocity;           // Linear velocity
    Vector2 force;              // Linear force (reset each step)
    float angularVelocity;      // Angular velocity
    float torque;               // Angular force (reset each step)
    float orient;               // Rotation in radians
    float inertia;              // Moment of inertia
    float inverseInertia;       // Inverse inertia
    float mass;                 // Body mass
    float inverseMass;          // Inverse mass
    float staticFriction;       // Friction at rest (0 to 1)
    float dynamicFriction;      // Friction in motion (0 to 1)
    float restitution;          // Bounciness (0 to 1)
    bool useGravity;            // Apply gravity
    bool isGrounded;            // Grounded state
    bool freezeOrient;          // Lock rotation
    PhysicsShape shape;         // Shape information
} PhysicsBodyData, *PhysicsBody;
```

### Functions

#### Initialization and Management

```c
// Initialize physics values and create physics loop thread
void InitPhysics(void);

// Run physics step manually (when PHYSAC_NO_THREADS is defined)
void RunPhysicsStep(void);

// Set physics fixed time step in milliseconds (default: 1.666666)
void SetPhysicsTimeStep(double delta);

// Check if physics thread is currently enabled
bool IsPhysicsEnabled(void);

// Set global gravity force
void SetPhysicsGravity(float x, float y);

// Clean up and close physics
void ClosePhysics(void);
```

#### Body Creation

```c
// Create a circle physics body
PhysicsBody CreatePhysicsBodyCircle(Vector2 pos, float radius, float density);

// Create a rectangle physics body
PhysicsBody CreatePhysicsBodyRectangle(Vector2 pos, float width, float height, float density);

// Create a regular polygon physics body
PhysicsBody CreatePhysicsBodyPolygon(Vector2 pos, float radius, int sides, float density);

// Destroy a physics body
void DestroyPhysicsBody(PhysicsBody body);
```

#### Force Application

```c
// Add linear force to a physics body
void PhysicsAddForce(PhysicsBody body, Vector2 force);

// Add angular force (torque) to a physics body
void PhysicsAddTorque(PhysicsBody body, float amount);

// Shatter a polygon body into smaller pieces with explosion force
void PhysicsShatter(PhysicsBody body, Vector2 position, float force);
```

#### Query Functions

```c
// Get current number of physics bodies
int GetPhysicsBodiesCount(void);

// Get physics body at index
PhysicsBody GetPhysicsBody(int index);

// Get shape type (PHYSICS_CIRCLE or PHYSICS_POLYGON)
int GetPhysicsShapeType(int index);

// Get vertex count of a shape
int GetPhysicsShapeVerticesCount(int index);

// Get transformed vertex position
Vector2 GetPhysicsShapeVertex(PhysicsBody body, int vertex);

// Set body rotation
void SetPhysicsBodyRotation(PhysicsBody body, float radians);
```

## Usage Example

```c
#define PHYSAC_IMPLEMENTATION
#define PHYSAC_STANDALONE
#include "physac.h"

int main(void)
{
    // Initialize physics
    InitPhysics();

    // Set gravity (default is 0, 9.81)
    SetPhysicsGravity(0.0f, 9.81f);

    // Create a static floor (density 0 = infinite mass = static)
    PhysicsBody floor = CreatePhysicsBodyRectangle(
        (Vector2){400, 550}, 800, 50, 0
    );
    floor->enabled = false;  // Disable dynamics for static body

    // Create a dynamic circle
    PhysicsBody ball = CreatePhysicsBodyCircle(
        (Vector2){400, 100}, 30, 1.0f
    );
    ball->restitution = 0.8f;  // Make it bouncy

    // Create a dynamic box
    PhysicsBody box = CreatePhysicsBodyRectangle(
        (Vector2){300, 200}, 50, 50, 1.0f
    );

    // Main loop
    while (running)
    {
        // Physics runs in its own thread (or call RunPhysicsStep() if NO_THREADS)

        // Use body positions for rendering
        Vector2 ballPos = ball->position;
        Vector2 boxPos = box->position;
        float boxRotation = box->orient;

        // Apply forces as needed
        if (keyPressed)
        {
            PhysicsAddForce(ball, (Vector2){100.0f, -200.0f});
        }
    }

    // Cleanup
    ClosePhysics();

    return 0;
}
```

## VOS/TCC Compatibility Notes

### Threading Considerations
- VOS may not support pthreads. Define `PHYSAC_NO_THREADS` and call `RunPhysicsStep()` manually in your game loop.

### Standalone Mode
- Define `PHYSAC_STANDALONE` to avoid raylib dependency. This provides internal Vector2 and bool definitions.

### Memory Management
- TCC supports standard malloc/free. Custom allocators can be defined via `PHYSAC_MALLOC` and `PHYSAC_FREE` if needed.

### Compilation
```c
#define PHYSAC_IMPLEMENTATION
#define PHYSAC_NO_THREADS
#define PHYSAC_STANDALONE
#include "physac.h"
```

### Performance Tips
- Reduce `PHYSAC_MAX_BODIES` and `PHYSAC_MAX_MANIFOLDS` if memory is limited
- Reduce `PHYSAC_COLLISION_ITERATIONS` for faster but less accurate simulation
- Use circles instead of polygons when possible (faster collision detection)

### Known Limitations
- Requires `<math.h>` functions (sqrtf, cosf, sinf, fabs)
- Uses `<time.h>` for timing (ensure VOS provides this)
- Default gravity is (0, 9.81) - adjust for your coordinate system
