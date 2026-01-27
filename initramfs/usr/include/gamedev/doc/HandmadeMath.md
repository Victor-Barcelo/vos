# HandmadeMath.h

A simple math library for games and computer graphics, compatible with both C and C++.

## Overview

HandmadeMath is a single-header math library designed for games and graphics applications. It provides a lightweight alternative to GLM that works in both C and C++ codebases. The library features vectors, matrices, and quaternions with all necessary arithmetic operations plus common graphics features like projection matrices and LookAt functions.

## Original Source

- **Repository**: https://github.com/HandmadeMath/HandmadeMath
- **Website**: https://handmade-math.handmade.network/
- **Version**: 2.0.0 (as included)
- **Original Author**: Zakary Strange

## License

**Public Domain** (where recognized) or perpetual, irrevocable license to copy, distribute, and modify.

This is one of the most permissive licenses available - you can use, modify, and distribute the library with no obligations.

## Features

- Single header file - no separate compilation needed
- Compatible with both C and C++ (with operator overloads in C++)
- Optional SIMD support (SSE on x86, NEON on ARM)
- Configurable angle units (radians, degrees, or turns)
- Left-handed and right-handed coordinate system variants
- Zero-to-one (ZO) and negative-one-to-one (NO) NDC variants
- C11 generic selection support (optional)
- Can work without the C runtime library

### Supported Types

| Type | Description |
|------|-------------|
| `HMM_Vec2` | 2D float vector |
| `HMM_Vec3` | 3D float vector |
| `HMM_Vec4` | 4D float vector |
| `HMM_Mat2` | 2x2 float matrix |
| `HMM_Mat3` | 3x3 float matrix |
| `HMM_Mat4` | 4x4 float matrix |
| `HMM_Quat` | Quaternion |
| `HMM_Bool` | Boolean (signed int) |

## API Reference

### Configuration Macros

```c
// Angle unit selection (default: radians)
#define HANDMADE_MATH_USE_RADIANS  // All angles in radians (default)
#define HANDMADE_MATH_USE_DEGREES  // All angles in degrees
#define HANDMADE_MATH_USE_TURNS    // All angles in turns (1 turn = 360 degrees)

// Disable SIMD
#define HANDMADE_MATH_NO_SIMD

// Provide custom math functions (for use without C runtime)
#define HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS
#define HMM_SINF MySinF
#define HMM_COSF MyCosF
#define HMM_TANF MyTanF
#define HMM_ACOSF MyACosF
#define HMM_SQRTF MySqrtF

// Custom angle conversion (if your math functions use different units)
#define HMM_ANGLE_USER_TO_INTERNAL(a) ((a)*HMM_DegToTurn)
#define HMM_ANGLE_INTERNAL_TO_USER(a) ((a)*HMM_TurnToDeg)
```

### Constants

```c
HMM_PI          // 3.14159265358979323846
HMM_PI32        // 3.14159265359f
HMM_RadToDeg    // Radians to degrees conversion factor
HMM_RadToTurn   // Radians to turns conversion factor
HMM_DegToRad    // Degrees to radians conversion factor
HMM_DegToTurn   // Degrees to turns conversion factor
HMM_TurnToRad   // Turns to radians conversion factor
HMM_TurnToDeg   // Turns to degrees conversion factor
```

### Utility Macros

```c
HMM_MIN(a, b)    // Minimum of two values
HMM_MAX(a, b)    // Maximum of two values
HMM_ABS(a)       // Absolute value
HMM_MOD(a, m)    // Modulo (handles negative numbers correctly)
HMM_SQUARE(x)    // Square a value
```

### Angle Unit Functions

```c
// Specify angles in a particular unit regardless of default
HMM_AngleRad(radians)   // Interpret as radians
HMM_AngleDeg(degrees)   // Interpret as degrees
HMM_AngleTurn(turns)    // Interpret as turns

// Convert between units
float HMM_ToRad(float Angle);   // Convert to radians
float HMM_ToDeg(float Angle);   // Convert to degrees
float HMM_ToTurn(float Angle);  // Convert to turns
```

### Math Functions

```c
float HMM_SinF(float Angle);
float HMM_CosF(float Angle);
float HMM_TanF(float Angle);
float HMM_ACosF(float Arg);
float HMM_SqrtF(float Float);
float HMM_InvSqrtF(float Float);   // 1 / sqrt(Float)
```

### Utility Functions

```c
float HMM_Lerp(float A, float Time, float B);     // Linear interpolation
float HMM_Clamp(float Min, float Value, float Max); // Clamp to range
```

### Vector Initialization

```c
HMM_Vec2 HMM_V2(float X, float Y);
HMM_Vec3 HMM_V3(float X, float Y, float Z);
HMM_Vec4 HMM_V4(float X, float Y, float Z, float W);
HMM_Vec4 HMM_V4V(HMM_Vec3 Vector, float W);  // Vec3 + W component
```

### Vector Operations

All vector operations return new vectors (immutable style):

```c
// Addition
HMM_Vec2 HMM_AddV2(HMM_Vec2 Left, HMM_Vec2 Right);
HMM_Vec3 HMM_AddV3(HMM_Vec3 Left, HMM_Vec3 Right);
HMM_Vec4 HMM_AddV4(HMM_Vec4 Left, HMM_Vec4 Right);

// Subtraction
HMM_Vec2 HMM_SubV2(HMM_Vec2 Left, HMM_Vec2 Right);
HMM_Vec3 HMM_SubV3(HMM_Vec3 Left, HMM_Vec3 Right);
HMM_Vec4 HMM_SubV4(HMM_Vec4 Left, HMM_Vec4 Right);

// Component-wise multiplication
HMM_Vec2 HMM_MulV2(HMM_Vec2 Left, HMM_Vec2 Right);
HMM_Vec3 HMM_MulV3(HMM_Vec3 Left, HMM_Vec3 Right);
HMM_Vec4 HMM_MulV4(HMM_Vec4 Left, HMM_Vec4 Right);

// Scalar multiplication
HMM_Vec2 HMM_MulV2F(HMM_Vec2 Left, float Right);
HMM_Vec3 HMM_MulV3F(HMM_Vec3 Left, float Right);
HMM_Vec4 HMM_MulV4F(HMM_Vec4 Left, float Right);

// Component-wise division
HMM_Vec2 HMM_DivV2(HMM_Vec2 Left, HMM_Vec2 Right);
HMM_Vec3 HMM_DivV3(HMM_Vec3 Left, HMM_Vec3 Right);
HMM_Vec4 HMM_DivV4(HMM_Vec4 Left, HMM_Vec4 Right);

// Scalar division
HMM_Vec2 HMM_DivV2F(HMM_Vec2 Left, float Right);
HMM_Vec3 HMM_DivV3F(HMM_Vec3 Left, float Right);
HMM_Vec4 HMM_DivV4F(HMM_Vec4 Left, float Right);

// Equality comparison
HMM_Bool HMM_EqV2(HMM_Vec2 Left, HMM_Vec2 Right);
HMM_Bool HMM_EqV3(HMM_Vec3 Left, HMM_Vec3 Right);
HMM_Bool HMM_EqV4(HMM_Vec4 Left, HMM_Vec4 Right);

// Dot product
float HMM_DotV2(HMM_Vec2 Left, HMM_Vec2 Right);
float HMM_DotV3(HMM_Vec3 Left, HMM_Vec3 Right);
float HMM_DotV4(HMM_Vec4 Left, HMM_Vec4 Right);

// Cross product (3D only)
HMM_Vec3 HMM_Cross(HMM_Vec3 Left, HMM_Vec3 Right);

// Length squared
float HMM_LenSqrV2(HMM_Vec2 A);
float HMM_LenSqrV3(HMM_Vec3 A);
float HMM_LenSqrV4(HMM_Vec4 A);

// Length (magnitude)
float HMM_LenV2(HMM_Vec2 A);
float HMM_LenV3(HMM_Vec3 A);
float HMM_LenV4(HMM_Vec4 A);

// Normalize
HMM_Vec2 HMM_NormV2(HMM_Vec2 A);
HMM_Vec3 HMM_NormV3(HMM_Vec3 A);
HMM_Vec4 HMM_NormV4(HMM_Vec4 A);

// Linear interpolation
HMM_Vec2 HMM_LerpV2(HMM_Vec2 A, float Time, HMM_Vec2 B);
HMM_Vec3 HMM_LerpV3(HMM_Vec3 A, float Time, HMM_Vec3 B);
HMM_Vec4 HMM_LerpV4(HMM_Vec4 A, float Time, HMM_Vec4 B);
```

### Vector Component Access

Vectors support multiple access patterns:

```c
HMM_Vec2 v2;
v2.X, v2.Y           // Position/general
v2.U, v2.V           // Texture coordinates
v2.Left, v2.Right    // UI
v2.Width, v2.Height  // Dimensions
v2.Elements[0]       // Array access

HMM_Vec3 v3;
v3.X, v3.Y, v3.Z     // Position
v3.R, v3.G, v3.B     // Color
v3.U, v3.V, v3.W     // Texture coordinates
v3.XY                // Swizzle to Vec2
v3.YZ                // Swizzle to Vec2

HMM_Vec4 v4;
v4.X, v4.Y, v4.Z, v4.W   // Position
v4.R, v4.G, v4.B, v4.A   // Color with alpha
v4.XYZ                    // Swizzle to Vec3
v4.RGB                    // Swizzle to Vec3
v4.XY, v4.YZ, v4.ZW      // Swizzle to Vec2
```

### Matrix Operations

```c
// Initialization
HMM_Mat2 HMM_M2(void);                    // Zero matrix
HMM_Mat2 HMM_M2D(float Diagonal);         // Diagonal matrix
HMM_Mat3 HMM_M3(void);
HMM_Mat3 HMM_M3D(float Diagonal);
HMM_Mat4 HMM_M4(void);
HMM_Mat4 HMM_M4D(float Diagonal);         // Use M4D(1.0f) for identity

// Transpose
HMM_Mat2 HMM_TransposeM2(HMM_Mat2 Matrix);
HMM_Mat3 HMM_TransposeM3(HMM_Mat3 Matrix);
HMM_Mat4 HMM_TransposeM4(HMM_Mat4 Matrix);

// Addition
HMM_Mat2 HMM_AddM2(HMM_Mat2 Left, HMM_Mat2 Right);
HMM_Mat3 HMM_AddM3(HMM_Mat3 Left, HMM_Mat3 Right);
HMM_Mat4 HMM_AddM4(HMM_Mat4 Left, HMM_Mat4 Right);

// Subtraction
HMM_Mat2 HMM_SubM2(HMM_Mat2 Left, HMM_Mat2 Right);
HMM_Mat3 HMM_SubM3(HMM_Mat3 Left, HMM_Mat3 Right);
HMM_Mat4 HMM_SubM4(HMM_Mat4 Left, HMM_Mat4 Right);

// Matrix multiplication
HMM_Mat2 HMM_MulM2(HMM_Mat2 Left, HMM_Mat2 Right);
HMM_Mat3 HMM_MulM3(HMM_Mat3 Left, HMM_Mat3 Right);
HMM_Mat4 HMM_MulM4(HMM_Mat4 Left, HMM_Mat4 Right);

// Matrix-scalar multiplication
HMM_Mat2 HMM_MulM2F(HMM_Mat2 Matrix, float Scalar);
HMM_Mat3 HMM_MulM3F(HMM_Mat3 Matrix, float Scalar);
HMM_Mat4 HMM_MulM4F(HMM_Mat4 Matrix, float Scalar);

// Matrix-vector multiplication
HMM_Vec2 HMM_MulM2V2(HMM_Mat2 Matrix, HMM_Vec2 Vector);
HMM_Vec3 HMM_MulM3V3(HMM_Mat3 Matrix, HMM_Vec3 Vector);
HMM_Vec4 HMM_MulM4V4(HMM_Mat4 Matrix, HMM_Vec4 Vector);

// Matrix-scalar division
HMM_Mat2 HMM_DivM2F(HMM_Mat2 Matrix, float Scalar);
HMM_Mat3 HMM_DivM3F(HMM_Mat3 Matrix, float Scalar);
HMM_Mat4 HMM_DivM4F(HMM_Mat4 Matrix, float Scalar);

// Determinant
float HMM_DeterminantM2(HMM_Mat2 Matrix);
float HMM_DeterminantM3(HMM_Mat3 Matrix);
float HMM_DeterminantM4(HMM_Mat4 Matrix);

// General inverse
HMM_Mat2 HMM_InvGeneralM2(HMM_Mat2 Matrix);
HMM_Mat3 HMM_InvGeneralM3(HMM_Mat3 Matrix);
HMM_Mat4 HMM_InvGeneralM4(HMM_Mat4 Matrix);

// Specialized fast inverse for specific matrix types
HMM_Mat4 HMM_InvOrthographic(HMM_Mat4 OrthoMatrix);
HMM_Mat4 HMM_InvPerspective_RH(HMM_Mat4 PerspectiveMatrix);
HMM_Mat4 HMM_InvPerspective_LH(HMM_Mat4 PerspectiveMatrix);
HMM_Mat4 HMM_InvTranslate(HMM_Mat4 TranslationMatrix);
```

### Transformation Matrices

```c
// Translation
HMM_Mat4 HMM_Translate(HMM_Vec3 Translation);

// Rotation (RH = right-handed, LH = left-handed)
HMM_Mat4 HMM_Rotate_RH(float Angle, HMM_Vec3 Axis);
HMM_Mat4 HMM_Rotate_LH(float Angle, HMM_Vec3 Axis);

// Scale
HMM_Mat4 HMM_Scale(HMM_Vec3 Scale);
```

### Projection Matrices

Naming convention: `_RH` = right-handed, `_LH` = left-handed, `_NO` = NDC -1 to 1, `_ZO` = NDC 0 to 1

```c
// Orthographic projection
HMM_Mat4 HMM_Orthographic_RH_NO(float Left, float Right, float Bottom, float Top, float Near, float Far);
HMM_Mat4 HMM_Orthographic_RH_ZO(float Left, float Right, float Bottom, float Top, float Near, float Far);
HMM_Mat4 HMM_Orthographic_LH_NO(float Left, float Right, float Bottom, float Top, float Near, float Far);
HMM_Mat4 HMM_Orthographic_LH_ZO(float Left, float Right, float Bottom, float Top, float Near, float Far);

// Perspective projection
HMM_Mat4 HMM_Perspective_RH_NO(float FOV, float AspectRatio, float Near, float Far);
HMM_Mat4 HMM_Perspective_RH_ZO(float FOV, float AspectRatio, float Near, float Far);
HMM_Mat4 HMM_Perspective_LH_NO(float FOV, float AspectRatio, float Near, float Far);
HMM_Mat4 HMM_Perspective_LH_ZO(float FOV, float AspectRatio, float Near, float Far);
```

### View Matrices

```c
HMM_Mat4 HMM_LookAt_RH(HMM_Vec3 Eye, HMM_Vec3 Center, HMM_Vec3 Up);
HMM_Mat4 HMM_LookAt_LH(HMM_Vec3 Eye, HMM_Vec3 Center, HMM_Vec3 Up);
```

### Quaternion Operations

```c
// Initialization
HMM_Quat HMM_Q(float X, float Y, float Z, float W);
HMM_Quat HMM_QV4(HMM_Vec4 Vector);

// Arithmetic
HMM_Quat HMM_AddQ(HMM_Quat Left, HMM_Quat Right);
HMM_Quat HMM_SubQ(HMM_Quat Left, HMM_Quat Right);
HMM_Quat HMM_MulQ(HMM_Quat Left, HMM_Quat Right);
HMM_Quat HMM_MulQF(HMM_Quat Left, float Multiplicand);
HMM_Quat HMM_DivQF(HMM_Quat Left, float Divident);

// Dot product
float HMM_DotQ(HMM_Quat Left, HMM_Quat Right);

// Inverse
HMM_Quat HMM_InvQ(HMM_Quat Left);

// Normalize
HMM_Quat HMM_NormQ(HMM_Quat Quat);

// Interpolation
HMM_Quat HMM_SLerp(HMM_Quat Left, float Time, HMM_Quat Right);

// Conversions
HMM_Mat4 HMM_QToM4(HMM_Quat Left);           // Quaternion to rotation matrix
HMM_Quat HMM_M4ToQ(HMM_Mat4 Matrix);         // Matrix to quaternion
HMM_Quat HMM_QFromAxisAngle_RH(HMM_Vec3 Axis, float AngleOfRotation);
HMM_Quat HMM_QFromAxisAngle_LH(HMM_Vec3 Axis, float AngleOfRotation);
```

## Usage Examples

### Basic Usage

```c
#include "HandmadeMath.h"

int main() {
    // Create vectors
    HMM_Vec3 a = HMM_V3(1.0f, 2.0f, 3.0f);
    HMM_Vec3 b = HMM_V3(4.0f, 5.0f, 6.0f);

    // Vector operations
    HMM_Vec3 sum = HMM_AddV3(a, b);
    HMM_Vec3 normalized = HMM_NormV3(a);
    float dot = HMM_DotV3(a, b);
    HMM_Vec3 cross = HMM_Cross(a, b);

    // Access components
    float x = a.X;
    float r = a.R;  // Same as X

    return 0;
}
```

### Setting Up a 3D Camera

```c
#include "HandmadeMath.h"

void setup_camera(HMM_Mat4 *mvp) {
    // Create perspective projection (OpenGL style, right-handed, NDC -1 to 1)
    HMM_Mat4 projection = HMM_Perspective_RH_NO(
        HMM_AngleDeg(45.0f),  // 45 degree FOV
        16.0f / 9.0f,         // Aspect ratio
        0.1f,                 // Near plane
        100.0f                // Far plane
    );

    // Create view matrix (camera at (0,0,5) looking at origin)
    HMM_Mat4 view = HMM_LookAt_RH(
        HMM_V3(0.0f, 0.0f, 5.0f),   // Eye position
        HMM_V3(0.0f, 0.0f, 0.0f),   // Target
        HMM_V3(0.0f, 1.0f, 0.0f)    // Up vector
    );

    // Create model matrix
    HMM_Mat4 model = HMM_M4D(1.0f);  // Identity
    model = HMM_MulM4(model, HMM_Translate(HMM_V3(0.0f, 0.0f, 0.0f)));
    model = HMM_MulM4(model, HMM_Rotate_RH(HMM_AngleDeg(45.0f), HMM_V3(0.0f, 1.0f, 0.0f)));
    model = HMM_MulM4(model, HMM_Scale(HMM_V3(1.0f, 1.0f, 1.0f)));

    // Combine: MVP = projection * view * model
    *mvp = HMM_MulM4(HMM_MulM4(projection, view), model);
}
```

### Quaternion Rotation Animation

```c
#include "HandmadeMath.h"

void animate_rotation(float t, HMM_Mat4 *rotation_matrix) {
    // Create start and end rotations
    HMM_Quat start = HMM_QFromAxisAngle_RH(HMM_V3(0.0f, 1.0f, 0.0f), HMM_AngleDeg(0.0f));
    HMM_Quat end = HMM_QFromAxisAngle_RH(HMM_V3(0.0f, 1.0f, 0.0f), HMM_AngleDeg(90.0f));

    // Spherical linear interpolation
    HMM_Quat current = HMM_SLerp(start, t, end);

    // Convert to matrix
    *rotation_matrix = HMM_QToM4(current);
}
```

### Using Degrees Instead of Radians

```c
// At the top of your file, before including HandmadeMath.h
#define HANDMADE_MATH_USE_DEGREES
#include "HandmadeMath.h"

void example() {
    // Now all functions accept degrees directly
    HMM_Mat4 rotation = HMM_Rotate_RH(45.0f, HMM_V3(0.0f, 1.0f, 0.0f));

    // Still can specify radians explicitly if needed
    HMM_Mat4 rotation2 = HMM_Rotate_RH(HMM_AngleRad(0.785f), HMM_V3(0.0f, 1.0f, 0.0f));
}
```

### C++ Operator Overloads

```cpp
// In C++, HandmadeMath provides operator overloads
#include "HandmadeMath.h"

void cpp_example() {
    HMM_Vec3 a = HMM_V3(1.0f, 2.0f, 3.0f);
    HMM_Vec3 b = HMM_V3(4.0f, 5.0f, 6.0f);

    // These work in C++
    HMM_Vec3 sum = a + b;
    HMM_Vec3 diff = a - b;
    HMM_Vec3 scaled = a * 2.0f;

    // Array-style access
    float x = a[0];
}
```

## VOS/TCC Compatibility Notes

HandmadeMath is specifically adapted for VOS and TCC in this version:

### Automatic SIMD Disabling

The included version automatically disables SIMD when compiled with TCC or for VOS:

```c
// VOS/TCC: Auto-disable SIMD (TCC doesn't support intrinsics)
#if defined(__TINYC__) || defined(__VOS__)
# ifndef HANDMADE_MATH_NO_SIMD
#  define HANDMADE_MATH_NO_SIMD
# endif
#endif
```

This means you don't need to manually disable SIMD - it happens automatically.

### VOS Usage

```c
// Just include and use - SIMD is auto-disabled for TCC/VOS
#include "HandmadeMath.h"

void render() {
    HMM_Mat4 mvp;
    // ... use all functions normally
}
```

### Key Compatibility Points

1. **SIMD Auto-Detection**: SIMD is automatically disabled for TCC and VOS builds
2. **All Functions Static Inline**: No linking required, just include the header
3. **Pure C Fallback**: When SIMD is disabled, uses portable C code
4. **No External Dependencies**: Self-contained (optionally needs `<math.h>`)

### Using Without C Runtime

If you need to use HandmadeMath without the C runtime library:

```c
// Provide your own math functions
#define HANDMADE_MATH_PROVIDE_MATH_FUNCTIONS
#define HMM_SINF my_sinf
#define HMM_COSF my_cosf
#define HMM_TANF my_tanf
#define HMM_ACOSF my_acosf
#define HMM_SQRTF my_sqrtf
#include "HandmadeMath.h"

// Implement your math functions
float my_sinf(float x) { /* ... */ }
float my_cosf(float x) { /* ... */ }
// etc.
```

### Potential Issues

1. **Anonymous Unions**: HandmadeMath uses anonymous unions/structs. TCC supports these.

2. **C11 Generics**: If using C11, the library can use `_Generic` for cleaner API. TCC has limited C11 support, but the library gracefully falls back.

3. **Pragma Warnings**: Some compiler-specific pragmas are used but are safely ignored by TCC.

## Conventions

### Matrix Layout

- **Column-major storage**: Data is stored by columns, then rows
- **Column vectors**: Vectors are written vertically
- **Multiplication order**: Matrix-vector multiplication is `M * V` (not `V * M`)

### Handedness

Functions that care about coordinate system handedness have variants:
- `_RH`: Right-handed coordinate system
- `_LH`: Left-handed coordinate system

### NDC Conventions

Projection functions have variants for different Normalized Device Coordinate conventions:
- `_NO`: NDC from -1 to 1 (OpenGL style)
- `_ZO`: NDC from 0 to 1 (Direct3D/Vulkan style)

## Migration from v1.x to v2.0

If you're coming from HandmadeMath 1.x:

1. **Consistent angle units**: v2.0 uses the same unit for all functions (configurable)
2. **New function naming**: Many functions were renamed for consistency
3. **Handedness variants**: v2.0 has explicit LH/RH variants where needed
4. **NDC variants**: v2.0 has explicit NO/ZO variants for projection matrices
