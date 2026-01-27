// Example: HandmadeMath.h - Game Math Library
// Compile: tcc ex_handmade_math.c -lm -o ex_handmade_math

#include <stdio.h>

// SIMD is auto-disabled for TCC via our patch
#include "../HandmadeMath.h"

int main(void) {
    printf("=== HandmadeMath.h Example ===\n\n");

    // 2D Vectors (great for 2D games)
    HMM_Vec2 pos = HMM_V2(100.0f, 200.0f);
    HMM_Vec2 vel = HMM_V2(5.0f, -3.0f);
    HMM_Vec2 new_pos = HMM_AddV2(pos, vel);
    printf("2D Position: (%.1f, %.1f) + vel = (%.1f, %.1f)\n",
           pos.X, pos.Y, new_pos.X, new_pos.Y);

    // 3D Vectors
    HMM_Vec3 a = HMM_V3(1.0f, 2.0f, 3.0f);
    HMM_Vec3 b = HMM_V3(4.0f, 5.0f, 6.0f);

    HMM_Vec3 sum = HMM_AddV3(a, b);
    printf("\n3D Add: (%.1f, %.1f, %.1f)\n", sum.X, sum.Y, sum.Z);

    HMM_Vec3 cross = HMM_Cross(a, b);
    printf("Cross: (%.1f, %.1f, %.1f)\n", cross.X, cross.Y, cross.Z);

    float dot = HMM_DotV3(a, b);
    printf("Dot: %.1f\n", dot);

    HMM_Vec3 norm = HMM_NormV3(a);
    printf("Normalized: (%.3f, %.3f, %.3f)\n", norm.X, norm.Y, norm.Z);

    // Quaternions
    printf("\n--- Quaternions ---\n");
    HMM_Quat q = HMM_QFromAxisAngle_RH(HMM_V3(0, 1, 0), HMM_PI / 4.0f);
    printf("Quat (45 deg Y): (%.3f, %.3f, %.3f, %.3f)\n", q.X, q.Y, q.Z, q.W);

    // Matrices
    printf("\n--- Matrices ---\n");
    HMM_Mat4 identity = HMM_M4D(1.0f);
    printf("Identity matrix created.\n");

    HMM_Mat4 trans = HMM_Translate(HMM_V3(10, 20, 30));
    printf("Translation (10, 20, 30) matrix created.\n");

    HMM_Mat4 rot = HMM_Rotate_RH(HMM_AngleDeg(45.0f), HMM_V3(0, 1, 0));
    printf("Rotation 45 deg Y matrix created.\n");

    HMM_Mat4 scale = HMM_Scale(HMM_V3(2, 2, 2));
    printf("Scale 2x matrix created.\n");

    // Combine: Scale -> Rotate -> Translate (read right to left)
    HMM_Mat4 model = HMM_MulM4(trans, HMM_MulM4(rot, scale));
    printf("Combined model matrix created.\n");

    // Perspective projection
    HMM_Mat4 proj = HMM_Perspective_RH_NO(HMM_AngleDeg(60.0f), 16.0f/9.0f, 0.1f, 100.0f);
    printf("Perspective projection created.\n");

    // Linear interpolation (great for animations)
    float t = 0.5f;
    float lerped = HMM_Lerp(0.0f, t, 100.0f);
    printf("\nLerp(0, 0.5, 100) = %.1f\n", lerped);

    printf("\nDone!\n");
    return 0;
}
