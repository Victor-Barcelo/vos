#include "small3d.h"

#include <math.h>

const uint8_t s3d_cube_edges[12][2] = {
    {0, 1},
    {1, 2},
    {2, 3},
    {3, 0},
    {4, 5},
    {5, 6},
    {6, 7},
    {7, 4},
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7},
};

static s3d_vec3_t rotate_xyz(s3d_vec3_t v, float ax, float ay, float az) {
    float sx = sinf(ax);
    float cx = cosf(ax);
    float sy = sinf(ay);
    float cy = cosf(ay);
    float sz = sinf(az);
    float cz = cosf(az);

    // Rotate around X.
    float y1 = v.y * cx - v.z * sx;
    float z1 = v.y * sx + v.z * cx;
    v.y = y1;
    v.z = z1;

    // Rotate around Y.
    float x2 = v.x * cy + v.z * sy;
    float z2 = -v.x * sy + v.z * cy;
    v.x = x2;
    v.z = z2;

    // Rotate around Z.
    float x3 = v.x * cz - v.y * sz;
    float y3 = v.x * sz + v.y * cz;
    v.x = x3;
    v.y = y3;

    return v;
}

void s3d_project_wire_cube(float angle_x, float angle_y, float angle_z,
                           float size,
                           int32_t screen_w, int32_t screen_h,
                           int32_t center_x, int32_t center_y,
                           s3d_point2i_t out_points[8]) {
    (void)screen_h;
    if (!out_points || screen_w <= 0) {
        return;
    }

    // Unit cube vertices (-1..1).
    static const s3d_vec3_t verts[8] = {
        {-1.0f, -1.0f, -1.0f},
        {+1.0f, -1.0f, -1.0f},
        {+1.0f, +1.0f, -1.0f},
        {-1.0f, +1.0f, -1.0f},
        {-1.0f, -1.0f, +1.0f},
        {+1.0f, -1.0f, +1.0f},
        {+1.0f, +1.0f, +1.0f},
        {-1.0f, +1.0f, +1.0f},
    };

    // Perspective parameters.
    float z_offset = 4.0f;
    float fov = (float)screen_w * 0.6f;

    for (int i = 0; i < 8; i++) {
        s3d_vec3_t v = verts[i];
        v = rotate_xyz(v, angle_x, angle_y, angle_z);
        v.x *= size;
        v.y *= size;
        v.z *= size;

        float z = v.z + z_offset * size;
        if (z < 1.0f) {
            z = 1.0f;
        }

        float inv = fov / z;
        int32_t sx = center_x + (int32_t)(v.x * inv);
        int32_t sy = center_y - (int32_t)(v.y * inv);
        out_points[i].x = sx;
        out_points[i].y = sy;
    }
}

