// Example: hypatia.h - Math Library (C89 compatible)
// Compile: tcc ex_hypatia.c -lm -o ex_hypatia

#include <stdio.h>

#define HYPATIA_IMPLEMENTATION
#include "../hypatia.h"

int main(void) {
    printf("=== hypatia.h Example ===\n\n");

    // 3D Vectors
    struct vector3 a = {1.0f, 2.0f, 3.0f};
    struct vector3 b = {4.0f, 5.0f, 6.0f};
    struct vector3 result;

    // Vector addition
    vector3_add(&result, &a, &b);
    printf("Add: (%.1f, %.1f, %.1f)\n", result.x, result.y, result.z);

    // Cross product
    vector3_cross_product(&result, &a, &b);
    printf("Cross: (%.1f, %.1f, %.1f)\n", result.x, result.y, result.z);

    // Dot product
    float dot = vector3_dot_product(&a, &b);
    printf("Dot: %.1f\n", dot);

    // Magnitude (length)
    float mag = vector3_magnitude(&a);
    printf("Magnitude of a: %.2f\n", mag);

    // Normalize
    vector3_normalize(&result, &a);
    printf("Normalized: (%.3f, %.3f, %.3f)\n", result.x, result.y, result.z);

    // Quaternions for rotation
    printf("\n--- Quaternions ---\n");
    struct quaternion q;
    quaternion_set_from_axis_anglef3(&q, 0, 1, 0, HYP_TAU / 8.0f);  // 45 deg around Y
    printf("Quaternion (45 deg Y): (%.3f, %.3f, %.3f, %.3f)\n", q.x, q.y, q.z, q.w);

    // Rotate a vector by quaternion
    struct vector3 v = {1, 0, 0};
    struct vector3 rotated;
    vector3_rotate_by_quaternion(&rotated, &v, &q);
    printf("(1,0,0) rotated 45 deg Y: (%.3f, %.3f, %.3f)\n", rotated.x, rotated.y, rotated.z);

    // 4x4 Matrix
    printf("\n--- Matrices ---\n");
    struct matrix4 mat;
    matrix4_identity(&mat);
    matrix4_translatef(&mat, 10, 20, 30);
    printf("Translation matrix created.\n");

    // Perspective projection
    struct matrix4 proj;
    matrix4_projection_perspective_fovyf(&proj, HYP_TAU / 6.0f, 16.0f/9.0f, 0.1f, 100.0f);
    printf("Perspective projection matrix created.\n");

    printf("\nDone!\n");
    return 0;
}
