# linmath.h

A lean linear math library aimed at graphics programming.

## Overview

linmath.h is a lightweight, single-header C library providing the most commonly used types required for programming computer graphics. The types are deliberately named like the types in GLSL (OpenGL Shading Language), allowing seamless client-side mathematical computations and straightforward data passing to identically-typed GLSL uniform variables.

## Original Source

- **Repository**: https://github.com/datenwolf/linmath.h
- **Author**: Wolfgang 'datenwolf' Draxinger
- **Also bundled with**: GLFW (https://github.com/glfw/glfw/blob/master/deps/linmath.h)

## License

WTFPL (Do What The Fuck You Want To Public License) Version 2

This is a highly permissive license that grants broad freedoms to users with minimal restrictions. You can use, modify, and distribute the library with essentially no obligations.

## Features

- Single header file - just include and use
- C99 compatible
- GLSL-like type naming
- Column-major matrix storage
- All angles in radians
- Static inline functions (can be changed with `LINMATH_NO_INLINE`)

### Supported Types

| Type | Description |
|------|-------------|
| `vec2` | 2-element float vector |
| `vec3` | 3-element float vector |
| `vec4` | 4-element float vector (with 4th component for homogeneous computations) |
| `mat4x4` | 4x4 matrix with column-major ordering |
| `quat` | Quaternion (4-element float array) |

## API Reference

### Configuration Macros

```c
#define LINMATH_NO_INLINE  // Use static functions instead of static inline
```

### Vector Operations (vec2, vec3, vec4)

All vector operations follow the pattern where the first parameter is the result:

```c
// Vector addition: r = a + b
void vec{n}_add(vec{n} r, vec{n} const a, vec{n} const b);

// Vector subtraction: r = a - b
void vec{n}_sub(vec{n} r, vec{n} const a, vec{n} const b);

// Scalar multiplication: r = v * s
void vec{n}_scale(vec{n} r, vec{n} const v, float const s);

// Dot product (inner product): returns a . b
float vec{n}_mul_inner(vec{n} const a, vec{n} const b);

// Vector length (magnitude)
float vec{n}_len(vec{n} const v);

// Normalize vector: r = v / |v|
void vec{n}_norm(vec{n} r, vec{n} const v);

// Component-wise minimum
void vec{n}_min(vec{n} r, vec{n} const a, vec{n} const b);

// Component-wise maximum
void vec{n}_max(vec{n} r, vec{n} const a, vec{n} const b);

// Copy vector
void vec{n}_dup(vec{n} r, vec{n} const src);
```

### vec3-specific Operations

```c
// Cross product: r = a x b
void vec3_mul_cross(vec3 r, vec3 const a, vec3 const b);

// Reflection: r = v - 2*(v.n)*n
void vec3_reflect(vec3 r, vec3 const v, vec3 const n);
```

### vec4-specific Operations

```c
// Cross product (for 3D part, sets w=1)
void vec4_mul_cross(vec4 r, vec4 const a, vec4 const b);

// Reflection
void vec4_reflect(vec4 r, vec4 const v, vec4 const n);
```

### Matrix Operations (mat4x4)

```c
// Set to identity matrix
void mat4x4_identity(mat4x4 M);

// Copy matrix
void mat4x4_dup(mat4x4 M, mat4x4 const N);

// Extract row/column
void mat4x4_row(vec4 r, mat4x4 const M, int i);
void mat4x4_col(vec4 r, mat4x4 const M, int i);

// Transpose
void mat4x4_transpose(mat4x4 M, mat4x4 const N);

// Matrix arithmetic
void mat4x4_add(mat4x4 M, mat4x4 const a, mat4x4 const b);
void mat4x4_sub(mat4x4 M, mat4x4 const a, mat4x4 const b);
void mat4x4_scale(mat4x4 M, mat4x4 const a, float k);
void mat4x4_scale_aniso(mat4x4 M, mat4x4 const a, float x, float y, float z);

// Matrix multiplication: M = a * b
void mat4x4_mul(mat4x4 M, mat4x4 const a, mat4x4 const b);

// Matrix-vector multiplication: r = M * v
void mat4x4_mul_vec4(vec4 r, mat4x4 const M, vec4 const v);

// Inversion
void mat4x4_invert(mat4x4 T, mat4x4 const M);

// Orthonormalize matrix
void mat4x4_orthonormalize(mat4x4 R, mat4x4 const M);
```

### Transformation Matrices

```c
// Translation matrix
void mat4x4_translate(mat4x4 T, float x, float y, float z);
void mat4x4_translate_in_place(mat4x4 M, float x, float y, float z);

// Rotation matrices (angle in radians)
void mat4x4_rotate(mat4x4 R, mat4x4 const M, float x, float y, float z, float angle);
void mat4x4_rotate_X(mat4x4 Q, mat4x4 const M, float angle);
void mat4x4_rotate_Y(mat4x4 Q, mat4x4 const M, float angle);
void mat4x4_rotate_Z(mat4x4 Q, mat4x4 const M, float angle);

// Outer product
void mat4x4_from_vec3_mul_outer(mat4x4 M, vec3 const a, vec3 const b);
```

### Projection Matrices

```c
// Frustum projection
void mat4x4_frustum(mat4x4 M, float l, float r, float b, float t, float n, float f);

// Orthographic projection
void mat4x4_ortho(mat4x4 M, float l, float r, float b, float t, float n, float f);

// Perspective projection (y_fov in radians)
void mat4x4_perspective(mat4x4 m, float y_fov, float aspect, float n, float f);

// Look-at view matrix
void mat4x4_look_at(mat4x4 m, vec3 const eye, vec3 const center, vec3 const up);
```

### Quaternion Operations

```c
// Quaternion aliases (use vec4 operations)
#define quat_add vec4_add
#define quat_sub vec4_sub
#define quat_norm vec4_norm
#define quat_scale vec4_scale
#define quat_mul_inner vec4_mul_inner

// Set to identity quaternion (0,0,0,1)
void quat_identity(quat q);

// Quaternion multiplication
void quat_mul(quat r, quat const p, quat const q);

// Conjugate
void quat_conj(quat r, quat const q);

// Create rotation quaternion from axis-angle
void quat_rotate(quat r, float angle, vec3 const axis);

// Rotate vector by quaternion
void quat_mul_vec3(vec3 r, quat const q, vec3 const v);

// Convert quaternion to matrix
void mat4x4_from_quat(mat4x4 M, quat const q);

// Multiply matrix by quaternion (orthogonal matrices only)
void mat4x4o_mul_quat(mat4x4 R, mat4x4 const M, quat const q);

// Extract quaternion from matrix
void quat_from_mat4x4(quat q, mat4x4 const M);

// Arcball rotation
void mat4x4_arcball(mat4x4 R, mat4x4 const M, vec2 const a, vec2 const b, float s);
```

## Usage Examples

### Basic Vector Operations

```c
#include "linmath.h"

int main() {
    vec3 a = {1.0f, 2.0f, 3.0f};
    vec3 b = {4.0f, 5.0f, 6.0f};
    vec3 result;

    // Add vectors
    vec3_add(result, a, b);  // result = {5, 7, 9}

    // Normalize
    vec3_norm(result, a);

    // Cross product
    vec3_mul_cross(result, a, b);

    // Dot product
    float dot = vec3_mul_inner(a, b);

    return 0;
}
```

### Creating a View-Projection Matrix

```c
#include "linmath.h"
#include <math.h>

void setup_camera(mat4x4 mvp) {
    mat4x4 projection, view, model;

    // Create perspective projection
    float fov = 45.0f * (M_PI / 180.0f);  // Convert to radians
    mat4x4_perspective(projection, fov, 16.0f/9.0f, 0.1f, 100.0f);

    // Create view matrix (camera at (0,0,5) looking at origin)
    vec3 eye = {0.0f, 0.0f, 5.0f};
    vec3 center = {0.0f, 0.0f, 0.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};
    mat4x4_look_at(view, eye, center, up);

    // Create model matrix with rotation
    mat4x4_identity(model);
    mat4x4_rotate_Y(model, model, 0.5f);

    // Combine: MVP = projection * view * model
    mat4x4 temp;
    mat4x4_mul(temp, view, model);
    mat4x4_mul(mvp, projection, temp);
}
```

### Quaternion Rotation

```c
#include "linmath.h"

void rotate_point(vec3 result, vec3 point, float angle, vec3 axis) {
    quat rotation;

    // Create rotation quaternion
    quat_rotate(rotation, angle, axis);

    // Rotate the point
    quat_mul_vec3(result, rotation, point);
}
```

## VOS/TCC Compatibility Notes

linmath.h is fully compatible with VOS and TCC (Tiny C Compiler):

1. **No SIMD Dependencies**: The library uses pure C math with no SSE/SIMD intrinsics
2. **C99 Compatible**: Uses standard C99 features that TCC supports
3. **Header-Only**: No linking required - just include the header
4. **Standard Math Library**: Requires `<math.h>` for `sqrtf`, `sinf`, `cosf`, `tanf`, `acos`

### VOS Usage

```c
// In your VOS application
#include "linmath.h"

// All functions are available immediately
mat4x4 transform;
mat4x4_identity(transform);
```

### Potential Issues

- **Inline Functions**: If TCC has issues with inline functions, define `LINMATH_NO_INLINE` before including:
  ```c
  #define LINMATH_NO_INLINE
  #include "linmath.h"
  ```

- **Math Library**: Ensure the math library is linked (usually automatic with TCC)

## Design Philosophy

The essential design idea is that for non-scalar types, the first parameter points to where the result goes and the other parameters are the operands. Scalar results (like dot product or length) are delivered through the function return value.

This design allows for efficient in-place operations and follows a consistent pattern throughout the API.
