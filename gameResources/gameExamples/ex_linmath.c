// Example: linmath.h - Linear Math Library
// Compile: tcc ex_linmath.c -lm -o ex_linmath

#include <stdio.h>
#include "../linmath.h"

int main(void) {
    printf("=== linmath.h Example ===\n\n");

    // 3D Vectors
    vec3 a = {1.0f, 2.0f, 3.0f};
    vec3 b = {4.0f, 5.0f, 6.0f};
    vec3 result;

    // Vector addition
    vec3_add(result, a, b);
    printf("Vector Add: (%.1f, %.1f, %.1f) + (%.1f, %.1f, %.1f) = (%.1f, %.1f, %.1f)\n",
           a[0], a[1], a[2], b[0], b[1], b[2], result[0], result[1], result[2]);

    // Cross product
    vec3_mul_cross(result, a, b);
    printf("Cross Product: (%.1f, %.1f, %.1f)\n", result[0], result[1], result[2]);

    // Dot product
    float dot = vec3_mul_inner(a, b);
    printf("Dot Product: %.1f\n", dot);

    // Vector length
    float len = vec3_len(a);
    printf("Length of a: %.2f\n", len);

    // Normalize
    vec3_norm(result, a);
    printf("Normalized a: (%.3f, %.3f, %.3f)\n", result[0], result[1], result[2]);

    // 4x4 Matrix - Identity
    mat4x4 identity;
    mat4x4_identity(identity);
    printf("\nIdentity Matrix:\n");
    for (int i = 0; i < 4; i++) {
        printf("  [%.0f %.0f %.0f %.0f]\n",
               identity[i][0], identity[i][1], identity[i][2], identity[i][3]);
    }

    // Translation matrix
    mat4x4 trans;
    mat4x4_translate(trans, 10.0f, 20.0f, 30.0f);
    printf("\nTranslation Matrix (10, 20, 30):\n");
    printf("  Position: (%.0f, %.0f, %.0f)\n", trans[3][0], trans[3][1], trans[3][2]);

    // Rotation matrix (around Y axis)
    mat4x4 rot;
    mat4x4_identity(rot);
    mat4x4_rotate_Y(rot, rot, 3.14159f / 4.0f);  // 45 degrees
    printf("\nRotation 45 deg around Y applied.\n");

    printf("\nDone!\n");
    return 0;
}
