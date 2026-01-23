#ifndef VOS_SMALL3D_H
#define VOS_SMALL3D_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct s3d_vec3 {
    float x;
    float y;
    float z;
} s3d_vec3_t;

typedef struct s3d_point2i {
    int32_t x;
    int32_t y;
} s3d_point2i_t;

// 12 edges of a cube as index pairs into the projected vertex array (size 8).
extern const uint8_t s3d_cube_edges[12][2];

// Project a unit cube centered at the origin into screen space.
// Angles are in radians. "size" is an approximate scale in screen pixels.
void s3d_project_wire_cube(float angle_x, float angle_y, float angle_z,
                           float size,
                           int32_t screen_w, int32_t screen_h,
                           int32_t center_x, int32_t center_y,
                           s3d_point2i_t out_points[8]);

#ifdef __cplusplus
}
#endif

#endif

