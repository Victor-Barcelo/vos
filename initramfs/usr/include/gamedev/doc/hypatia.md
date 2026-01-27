# Hypatia

A header-only, pure-C math library for 2D/3D graphics programming.

## Overview

Hypatia is a single-file-header, pure-C math library designed for 2D/3D graphics applications (such as games). It provides comprehensive support for vectors, matrices, and quaternions with a focus on C89/C90 compliance for maximum portability.

The library is named after Hypatia of Alexandria, a Greek mathematician and philosopher.

## Original Source

- **Repository**: https://github.com/dagostinelli/hypatia
- **Documentation**: https://dagostinelli.github.io/hypatia/
- **Author**: Dennis Agostinelli
- **Version**: 2.0.0 (as included)

## License

MIT License (SPDX-License-Identifier: MIT)

## Features

- Single header file distribution
- Near C89/C90 compliance for maximum portability
- Supports both single and double precision floats
- Mutable math objects for performance
- Verbose function naming with short aliases available
- Comprehensive 2D, 3D, and 4D vector support
- 2x2, 3x3, and 4x4 matrix operations
- Full quaternion support with SLERP interpolation
- Transformation matrices (translation, rotation, scaling)
- Projection matrices (perspective, orthographic)
- View matrix (look-at)

### Supported Types

| Type | Description | Short Alias |
|------|-------------|-------------|
| `struct vector2` | 2D vector | `vec2` |
| `struct vector3` | 3D vector | `vec3` |
| `struct vector4` | 4D vector | `vec4` |
| `struct matrix2` | 2x2 matrix | `mat2` |
| `struct matrix3` | 3x3 matrix | `mat3` |
| `struct matrix4` | 4x4 matrix | `mat4` |
| `struct quaternion` | Quaternion | `quat` |

## API Reference

### Configuration Macros

```c
// Use single precision floats (default is double)
#define HYPATIA_SINGLE_PRECISION_FLOATS

// Make all functions static
#define HYP_STATIC

// Provide custom implementations
#define HYP_NO_C_MATH    // Don't include <math.h>
#define HYP_NO_STDIO     // Don't include <stdio.h>
#define HYP_MEMSET(a,b,c)  // Custom memset
#define HYP_SQRT(n)        // Custom sqrt
```

### Constants

```c
HYP_PI              // Pi to high precision
HYP_TAU             // 2 * Pi
HYP_PI_HALF         // Pi / 2
HYP_PI_SQUARED      // Pi^2
HYP_E               // Euler's number
HYP_RAD_PER_DEG     // Radians per degree (PI/180)
HYP_DEG_PER_RAD     // Degrees per radian (180/PI)
HYP_EPSILON         // Comparison tolerance (1E-5)
```

### Utility Macros/Functions

```c
HYP_MIN(a, b)              // Minimum of two values
HYP_MAX(a, b)              // Maximum of two values
HYP_SWAP(a, b)             // Swap two values
HYP_ABS(value)             // Absolute value
HYP_SQUARE(number)         // Square a number
HYP_SQRT(number)           // Square root
HYP_CLAMP(value, min, max) // Clamp value to range
HYP_WRAP(value, start, limit) // Wrap value in range
HYP_DEG_TO_RAD(angle)      // Convert degrees to radians
HYP_RAD_TO_DEG(radians)    // Convert radians to degrees
HYP_RANDOM_FLOAT           // Random float in [-1, 1]
```

### Vector2 Operations

```c
// Initialization and assignment
struct vector2 *vector2_zero(struct vector2 *self);
struct vector2 *vector2_set(struct vector2 *self, const struct vector2 *vT);
struct vector2 *vector2_setf2(struct vector2 *self, HYP_FLOAT x, HYP_FLOAT y);

// Comparison
int vector2_equals(const struct vector2 *self, const struct vector2 *vT);

// Arithmetic (modifies self)
struct vector2 *vector2_negate(struct vector2 *self);
struct vector2 *vector2_add(struct vector2 *self, const struct vector2 *vT);
struct vector2 *vector2_addf(struct vector2 *self, HYP_FLOAT f);
struct vector2 *vector2_subtract(struct vector2 *self, const struct vector2 *vT);
struct vector2 *vector2_subtractf(struct vector2 *self, HYP_FLOAT f);
struct vector2 *vector2_multiply(struct vector2 *self, const struct vector2 *vT);
struct vector2 *vector2_multiplyf(struct vector2 *self, HYP_FLOAT f);
struct vector2 *vector2_divide(struct vector2 *self, const struct vector2 *vT);
struct vector2 *vector2_dividef(struct vector2 *self, HYP_FLOAT f);

// Matrix multiplication
struct vector2 *vector2_multiplym2(struct vector2 *self, const struct matrix2 *mT);
struct vector2 *vector2_multiplym3(struct vector2 *self, const struct matrix3 *mT);

// Geometric operations
struct vector2 *vector2_normalize(struct vector2 *self);
HYP_FLOAT vector2_magnitude(const struct vector2 *self);
HYP_FLOAT vector2_length(const struct vector2 *self);  // alias for magnitude
HYP_FLOAT vector2_distance(const struct vector2 *v1, const struct vector2 *v2);
HYP_FLOAT vector2_dot_product(const struct vector2 *self, const struct vector2 *vT);
struct vector2 *vector2_cross_product(struct vector2 *vR, const struct vector2 *vT1, const struct vector2 *vT2);
HYP_FLOAT vector2_angle_between(const struct vector2 *self, const struct vector2 *vT);
struct vector2 *vector2_find_normal_axis_between(struct vector2 *vR, const struct vector2 *vT1, const struct vector2 *vT2);
```

### Vector3 Operations

```c
// All vector2 operations plus:
struct vector3 *vector3_setf3(struct vector3 *self, HYP_FLOAT x, HYP_FLOAT y, HYP_FLOAT z);
struct vector3 *vector3_multiplym4(struct vector3 *self, const struct matrix4 *mT);
struct vector3 *vector3_cross_product(struct vector3 *vR, const struct vector3 *vT1, const struct vector3 *vT2);

// Quaternion operations
struct vector3 *vector3_rotate_by_quaternion(struct vector3 *self, const struct quaternion *qT);
struct vector3 *vector3_reflect_by_quaternion(struct vector3 *self, const struct quaternion *qT);
```

### Vector4 Operations

```c
struct vector4 *vector4_setf4(struct vector4 *self, HYP_FLOAT x, HYP_FLOAT y, HYP_FLOAT z, HYP_FLOAT w);
// Plus standard arithmetic, normalize, magnitude, dot product, cross product
```

### Matrix2 Operations

```c
// Initialization
struct matrix2 *matrix2_zero(struct matrix2 *self);
struct matrix2 *matrix2_identity(struct matrix2 *self);
struct matrix2 *matrix2_set(struct matrix2 *self, const struct matrix2 *mT);

// Comparison
int matrix2_equals(const struct matrix2 *self, const struct matrix2 *mT);

// Arithmetic
struct matrix2 *matrix2_add(struct matrix2 *self, const struct matrix2 *mT);
struct matrix2 *matrix2_subtract(struct matrix2 *self, const struct matrix2 *mT);
struct matrix2 *matrix2_multiply(struct matrix2 *self, const struct matrix2 *mT);
struct matrix2 *matrix2_multiplyf(struct matrix2 *self, HYP_FLOAT scalar);
struct vector2 *matrix2_multiplyv2(const struct matrix2 *self, const struct vector2 *vT, struct vector2 *vR);

// Matrix operations
struct matrix2 *matrix2_transpose(struct matrix2 *self);
HYP_FLOAT matrix2_determinant(const struct matrix2 *self);
struct matrix2 *matrix2_invert(struct matrix2 *self);
struct matrix2 *matrix2_inverse(const struct matrix2 *self, struct matrix2 *mR);

// Transformations
struct matrix2 *matrix2_make_transformation_scalingv2(struct matrix2 *self, const struct vector2 *scale);
struct matrix2 *matrix2_make_transformation_rotationf_z(struct matrix2 *self, HYP_FLOAT angle);
struct matrix2 *matrix2_rotate(struct matrix2 *self, HYP_FLOAT angle);
struct matrix2 *matrix2_scalev2(struct matrix2 *self, const struct vector2 *scale);
```

### Matrix3 Operations

```c
// All matrix2 operations plus:
struct vector2 *matrix3_multiplyv2(const struct matrix3 *self, const struct vector2 *vT, struct vector2 *vR);

// Transformations
struct matrix3 *matrix3_make_transformation_translationv2(struct matrix3 *self, const struct vector2 *translation);
struct matrix3 *matrix3_make_transformation_scalingv2(struct matrix3 *self, const struct vector2 *scale);
struct matrix3 *matrix3_make_transformation_rotationf_z(struct matrix3 *self, HYP_FLOAT angle);
struct matrix3 *matrix3_translatev2(struct matrix3 *self, const struct vector2 *translation);
struct matrix3 *matrix3_rotate(struct matrix3 *self, HYP_FLOAT angle);
struct matrix3 *matrix3_scalev2(struct matrix3 *self, const struct vector2 *scale);
```

### Matrix4 Operations

```c
// Initialization and arithmetic (same pattern as matrix2/3)
struct matrix4 *matrix4_zero(struct matrix4 *self);
struct matrix4 *matrix4_identity(struct matrix4 *self);
struct matrix4 *matrix4_multiply(struct matrix4 *self, const struct matrix4 *mT);
struct matrix4 *matrix4_transpose(struct matrix4 *self);
HYP_FLOAT matrix4_determinant(const struct matrix4 *self);
struct matrix4 *matrix4_invert(struct matrix4 *self);

// Vector multiplication
struct vector4 *matrix4_multiplyv4(const struct matrix4 *self, const struct vector4 *vT, struct vector4 *vR);
struct vector3 *matrix4_multiplyv3(const struct matrix4 *self, const struct vector3 *vT, struct vector3 *vR);
struct vector2 *matrix4_multiplyv2(const struct matrix4 *self, const struct vector2 *vT, struct vector2 *vR);

// Transformation creation
struct matrix4 *matrix4_make_transformation_translationv3(struct matrix4 *self, const struct vector3 *translation);
struct matrix4 *matrix4_make_transformation_scalingv3(struct matrix4 *self, const struct vector3 *scale);
struct matrix4 *matrix4_make_transformation_rotationq(struct matrix4 *self, const struct quaternion *qT);
struct matrix4 *matrix4_make_transformation_rotationf_x(struct matrix4 *self, HYP_FLOAT angle);
struct matrix4 *matrix4_make_transformation_rotationf_y(struct matrix4 *self, HYP_FLOAT angle);
struct matrix4 *matrix4_make_transformation_rotationf_z(struct matrix4 *self, HYP_FLOAT angle);

// In-place transformations
struct matrix4 *matrix4_translatev3(struct matrix4 *self, const struct vector3 *translation);
struct matrix4 *matrix4_rotatev3(struct matrix4 *self, const struct vector3 *axis, HYP_FLOAT angle);
struct matrix4 *matrix4_scalev3(struct matrix4 *self, const struct vector3 *scale);
```

### Quaternion Operations

```c
// Initialization
struct quaternion *quaternion_identity(struct quaternion *self);
struct quaternion *quaternion_setf4(struct quaternion *self, HYP_FLOAT x, HYP_FLOAT y, HYP_FLOAT z, HYP_FLOAT w);
struct quaternion *quaternion_set(struct quaternion *self, const struct quaternion *qT);

// Comparison
int quaternion_equals(const struct quaternion *self, const struct quaternion *vT);

// Arithmetic
struct quaternion *quaternion_add(struct quaternion *self, const struct quaternion *qT);
struct quaternion *quaternion_subtract(struct quaternion *self, const struct quaternion *qT);
struct quaternion *quaternion_multiply(struct quaternion *self, const struct quaternion *qT);
struct quaternion *quaternion_multiplyv3(struct quaternion *self, const struct vector3 *vT);
struct quaternion *quaternion_multiplyf(struct quaternion *self, HYP_FLOAT f);
struct quaternion *quaternion_negate(struct quaternion *self);
struct quaternion *quaternion_conjugate(struct quaternion *self);
struct quaternion *quaternion_inverse(struct quaternion *self);

// Properties
short quaternion_is_unit(struct quaternion *self);
short quaternion_is_pure(struct quaternion *self);
HYP_FLOAT quaternion_norm(const struct quaternion *self);
HYP_FLOAT quaternion_magnitude(const struct quaternion *self);
struct quaternion *quaternion_normalize(struct quaternion *self);
HYP_FLOAT quaternion_dot_product(const struct quaternion *self, const struct quaternion *qT);

// Interpolation
struct quaternion *quaternion_lerp(const struct quaternion *start, const struct quaternion *end, HYP_FLOAT percent, struct quaternion *qR);
struct quaternion *quaternion_nlerp(const struct quaternion *start, const struct quaternion *end, HYP_FLOAT percent, struct quaternion *qR);
struct quaternion *quaternion_slerp(const struct quaternion *start, const struct quaternion *end, HYP_FLOAT percent, struct quaternion *qR);

// Conversion
void quaternion_get_axis_anglev3(const struct quaternion *self, struct vector3 *vR, HYP_FLOAT *angle);
struct quaternion *quaternion_set_from_axis_anglev3(struct quaternion *self, const struct vector3 *axis, HYP_FLOAT angle);
struct quaternion *quaternion_set_from_axis_anglef3(struct quaternion *self, HYP_FLOAT x, HYP_FLOAT y, HYP_FLOAT z, HYP_FLOAT angle);
struct quaternion *quaternion_set_from_euler_anglesf3(struct quaternion *self, HYP_FLOAT ax, HYP_FLOAT ay, HYP_FLOAT az);
void quaternion_get_euler_anglesf3(const struct quaternion *self, HYP_FLOAT *ax, HYP_FLOAT *ay, HYP_FLOAT *az);

// Utility
struct quaternion *quaternion_get_rotation_tov3(const struct vector3 *from, const struct vector3 *to, struct quaternion *qR);
```

### Experimental Functions (EXP suffix)

```c
// Projection matrices (right-handed)
struct matrix4 *matrix4_projection_perspective_fovy_rh_EXP(struct matrix4 *self, HYP_FLOAT fovy, HYP_FLOAT aspect, HYP_FLOAT zNear, HYP_FLOAT zFar);
struct matrix4 *matrix4_projection_ortho3d_rh_EXP(struct matrix4 *self, HYP_FLOAT xmin, HYP_FLOAT xmax, HYP_FLOAT ymin, HYP_FLOAT ymax, HYP_FLOAT zNear, HYP_FLOAT zFar);

// View matrix
struct matrix4 *matrix4_view_lookat_rh_EXP(struct matrix4 *self, const struct vector3 *eye, const struct vector3 *target, const struct vector3 *up);

// Quaternion rotation functions
struct quaternion *quaternion_rotate_by_quaternion_EXP(struct quaternion *self, const struct quaternion *qT);
struct quaternion *quaternion_rotate_by_axis_angle_EXP(struct quaternion *self, const struct vector3 *axis, HYP_FLOAT angle);
struct quaternion *quaternion_rotate_by_euler_angles_EXP(struct quaternion *self, HYP_FLOAT ax, HYP_FLOAT ay, HYP_FLOAT az);

// Matrix-quaternion conversion
struct matrix4 *matrix4_set_from_quaternion_EXP(struct matrix4 *self, const struct quaternion *qT);
struct matrix4 *matrix4_set_from_axisv3_angle_EXP(struct matrix4 *self, const struct vector3 *axis, HYP_FLOAT angle);

// Transformation composition/decomposition
struct matrix4 *matrix4_transformation_compose_EXP(struct matrix4 *self, const struct vector3 *scale, const struct quaternion *rotation, const struct vector3 *translation);
uint8_t matrix4_transformation_decompose_EXP(struct matrix4 *self, struct vector3 *scale, struct quaternion *rotation, struct vector3 *translation);
```

### Reference Vectors

Pre-defined constant vectors accessible via macros:

```c
HYP_VECTOR2_ZERO           // {0, 0}
HYP_VECTOR2_ONE            // {1, 1}
HYP_VECTOR2_UNIT_X         // {1, 0}
HYP_VECTOR2_UNIT_Y         // {0, 1}
HYP_VECTOR2_UNIT_X_NEGATIVE // {-1, 0}
HYP_VECTOR2_UNIT_Y_NEGATIVE // {0, -1}

HYP_VECTOR3_ZERO           // {0, 0, 0}
HYP_VECTOR3_ONE            // {1, 1, 1}
HYP_VECTOR3_UNIT_X         // {1, 0, 0}
HYP_VECTOR3_UNIT_Y         // {0, 1, 0}
HYP_VECTOR3_UNIT_Z         // {0, 0, 1}
HYP_VECTOR3_UNIT_X_NEGATIVE // {-1, 0, 0}
HYP_VECTOR3_UNIT_Y_NEGATIVE // {0, -1, 0}
HYP_VECTOR3_UNIT_Z_NEGATIVE // {0, 0, -1}
```

## Usage Examples

### Header Mode vs Implementation Mode

```c
// In most files, just include the header (declarations only)
#include "hypatia.h"

// In ONE .c file, define HYPATIA_IMPLEMENTATION to include the code
#define HYPATIA_IMPLEMENTATION
#include "hypatia.h"
```

### Basic Vector Operations

```c
#define HYPATIA_IMPLEMENTATION
#include "hypatia.h"

int main() {
    struct vector3 a, b, result;

    // Initialize vectors
    vector3_setf3(&a, 1.0, 2.0, 3.0);
    vector3_setf3(&b, 4.0, 5.0, 6.0);

    // Add vectors (modifies a)
    vector3_set(&result, &a);
    vector3_add(&result, &b);  // result = {5, 7, 9}

    // Normalize
    vector3_normalize(&result);

    // Dot product
    double dot = vector3_dot_product(&a, &b);

    // Cross product
    struct vector3 cross;
    vector3_cross_product(&cross, &a, &b);

    return 0;
}
```

### Creating a View-Projection Matrix

```c
#define HYPATIA_IMPLEMENTATION
#include "hypatia.h"

void setup_camera() {
    struct matrix4 projection, view, mvp;
    struct vector3 eye, target, up;

    // Create perspective projection
    matrix4_projection_perspective_fovy_rh_EXP(&projection,
        HYP_DEG_TO_RAD(45.0),  // 45 degree FOV
        16.0/9.0,              // Aspect ratio
        0.1,                   // Near plane
        100.0                  // Far plane
    );

    // Set up camera position and target
    vector3_setf3(&eye, 0.0, 0.0, 5.0);
    vector3_setf3(&target, 0.0, 0.0, 0.0);
    vector3_setf3(&up, 0.0, 1.0, 0.0);

    // Create view matrix
    matrix4_view_lookat_rh_EXP(&view, &eye, &target, &up);

    // Combine matrices
    matrix4_set(&mvp, &projection);
    matrix4_multiply(&mvp, &view);
}
```

### Quaternion Rotation with SLERP

```c
#define HYPATIA_IMPLEMENTATION
#include "hypatia.h"

void animate_rotation(float t) {
    struct quaternion start, end, current;
    struct vector3 axis;

    // Set up start rotation (identity)
    quaternion_identity(&start);

    // Set up end rotation (90 degrees around Y axis)
    vector3_setf3(&axis, 0.0, 1.0, 0.0);
    quaternion_set_from_axis_anglev3(&end, &axis, HYP_DEG_TO_RAD(90.0));

    // Interpolate (t goes from 0 to 1)
    quaternion_slerp(&start, &end, t, &current);

    // Convert to matrix for rendering
    struct matrix4 rotation_matrix;
    matrix4_set_from_quaternion_EXP(&rotation_matrix, &current);
}
```

### Using Short Aliases

```c
#define HYPATIA_IMPLEMENTATION
#include "hypatia.h"

int main() {
    // Using short type aliases
    vec3 position;
    mat4 transform;
    quat rotation;

    vector3_setf3(&position, 1.0, 2.0, 3.0);
    matrix4_identity(&transform);
    quaternion_identity(&rotation);

    return 0;
}
```

## VOS/TCC Compatibility Notes

Hypatia is well-suited for VOS and TCC environments:

1. **C89/C90 Compliance**: The library targets near-C89/C90 compliance, which TCC fully supports
2. **No SIMD Dependencies**: Pure C implementation with no SSE/NEON intrinsics
3. **Single Header**: Easy integration - just one file to include
4. **Configurable Precision**: Can use single or double precision floats

### VOS Usage

```c
// Option 1: Static functions (recommended for VOS)
#define HYP_STATIC
#define HYPATIA_IMPLEMENTATION
#include "hypatia.h"

// Option 2: Separate compilation
// In one .c file:
#define HYPATIA_IMPLEMENTATION
#include "hypatia.h"

// In other files, just include without HYPATIA_IMPLEMENTATION
```

### Potential Issues and Solutions

1. **`<memory.h>` Include**: If TCC lacks `<memory.h>`, provide a custom memset:
   ```c
   #define HYP_MEMSET(a, b, c) your_memset(a, b, c)
   ```

2. **Anonymous Unions**: Hypatia uses anonymous unions in struct definitions. TCC supports these in C99/C11 mode.

3. **Double Precision by Default**: For better performance on VOS, use single precision:
   ```c
   #define HYPATIA_SINGLE_PRECISION_FLOATS
   #include "hypatia.h"
   ```

4. **Implementation Mode**: Remember to define `HYPATIA_IMPLEMENTATION` in exactly one source file to include the function implementations.

## Design Philosophy

Hypatia uses **mutable objects** for performance - all operations modify the first parameter (`self`) in place and return a pointer to it for chaining. This was a purposeful design choice to minimize memory allocations and copies.

Example of chaining:
```c
vector3_normalize(vector3_add(&v1, &v2));
```

The verbose naming convention (`vector3_add` instead of `v3_add`) was chosen for clarity, though short aliases are provided for convenience.
