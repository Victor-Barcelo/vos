# SATC - SAT Collision Detection Library

## Description

SATC is a Separating Axis Theorem (SAT) collision detection library written in plain C99 as a single header file with zero dependencies. It is a port of the JavaScript library [sat-js](https://github.com/jriecken/sat-js).

This library handles 2D collision detection and overlap calculation between circles and convex polygons. It is **not a physics library** - friction, restitution, drag, velocity, acceleration, etc. must be handled separately if needed.

## Original Source

- **Repository**: [https://github.com/rjungemann/satc](https://github.com/rjungemann/satc)
- **Author**: Roger Jungemann
- **Original JavaScript Version**: [https://github.com/jriecken/sat-js](https://github.com/jriecken/sat-js)

## License

**MIT License**

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files, to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software.

## Features

- Single header file implementation
- Zero external dependencies
- Written in C99
- Compiles cleanly with `-Wall` (no warnings)
- 100% documentation coverage
- Nearly 100% test coverage
- Stack-based memory allocation using `alloca` for temporary variables
- Double-precision floating point for accuracy
- Supports collision detection between:
  - Circle vs Circle
  - Circle vs Polygon
  - Polygon vs Circle
  - Polygon vs Polygon
  - Point vs Circle
  - Point vs Polygon
- Provides collision response data (overlap amount, direction, containment)
- Axis-Aligned Bounding Box (AABB) calculation for shapes
- Polygon centroid calculation

## API Reference

### Type Definitions

```c
// Shape type identifiers
#define satc_type_none     0
#define satc_type_circle   1
#define satc_type_polygon  2
#define satc_type_box      3

// Voronoi region identifiers
#define SATC_LEFT_VORONOI_REGION   -1
#define SATC_MIDDLE_VORONOI_REGION  0
#define SATC_RIGHT_VORONOI_REGION   1

// Point array indices
#define SATC_POINT_X  0
#define SATC_POINT_Y  1
```

### Structures

#### satc_circle_t
```c
struct satc_circle {
    int type;       // Shape type (satc_type_circle)
    double *pos;    // Position [x, y]
    double r;       // Radius
};
```

#### satc_polygon_t
```c
struct satc_polygon {
    int type;               // Shape type (satc_type_polygon)
    double *pos;            // Position [x, y]
    size_t num_points;      // Number of vertices
    double **points;        // Vertex positions
    double angle;           // Rotation angle
    double *offset;         // Position offset
    size_t num_calc_points; // Calculated points count
    double **calc_points;   // Transformed vertices
    size_t num_edges;       // Edge count
    double **edges;         // Edge vectors
    size_t num_normals;     // Normal count
    double **normals;       // Edge normals
};
```

#### satc_box_t
```c
struct satc_box {
    int type;       // Shape type (satc_type_box)
    double *pos;    // Position [x, y] (top-left)
    double w;       // Width
    double h;       // Height
};
```

#### satc_response_t
```c
struct satc_response {
    void *a;            // First shape in collision
    void *b;            // Second shape in collision
    double overlap;     // Overlap distance
    double *overlap_n;  // Overlap unit vector
    double *overlap_v;  // Overlap vector (scaled by distance)
    bool a_in_b;        // True if A is entirely inside B
    bool b_in_a;        // True if B is entirely inside A
};
```

### Point Macros and Functions

#### Point Allocation (Stack)
```c
// Allocate point with undefined values
satc_point_alloca(name);

// Allocate point with x, y values
satc_point_alloca_xy(name, x, y);

// Allocate array of points
satc_point_array_alloca(name, size);

// Allocate double array
satc_double_array_alloca(name, size);
```

#### Point Access
```c
// Get/set individual coordinates
double x = satc_point_get_x(p);
double y = satc_point_get_y(p);
satc_point_set_x(p, value);
satc_point_set_y(p, value);
satc_point_set_xy(p, x, y);
```

#### Point Operations
```c
// Create point on heap (must be freed)
double *satc_point_create(double x, double y);

// Destroy heap-allocated point
void satc_point_destroy(double *point);

// Clone a point
#define satc_point_clone(p)

// Copy values from q to p
double *satc_point_copy(double *p, double *q);

// Vector operations (mutate p)
double *satc_point_add(double *p, double *q);       // p = p + q
double *satc_point_sub(double *p, double *q);       // p = p - q
double *satc_point_scale_x(double *p, double x);    // p = p * x
double *satc_point_scale_xy(double *p, double x, double y);
double *satc_point_rotate(double *p, double angle);
double *satc_point_normalize(double *p);
double *satc_point_perp(double *p);                 // Perpendicular
double *satc_point_reverse(double *p);              // Negate

// Vector calculations
#define satc_point_dot(p, q)    // Dot product
#define satc_point_len(p)       // Length
#define satc_point_len2(p)      // Length squared

// Projection
double *satc_point_project(double *p, double *q);
double *satc_point_project_n(double *p, double *q);
double *satc_point_reflect(double *p, double *axis);
double *satc_point_reflect_n(double *p, double *axis);
```

### Shape Creation and Destruction

#### Circles
```c
// Create circle (pos is cloned)
satc_circle_t *satc_circle_create(double *pos, double r);

// Destroy circle
void satc_circle_destroy(satc_circle_t *circle);

// Get bounding box as polygon
satc_polygon_t *satc_circle_get_aabb(satc_circle_t *circle);
```

#### Polygons
```c
// Create polygon (pos and points are cloned)
satc_polygon_t *satc_polygon_create(double *pos, size_t num_points, double **points);

// Destroy polygon
void satc_polygon_destroy(satc_polygon_t *polygon);

// Modify polygon
satc_polygon_t *satc_polygon_set_points(satc_polygon_t *polygon, size_t num_points, double **points);
satc_polygon_t *satc_polygon_set_angle(satc_polygon_t *polygon, double angle);
satc_polygon_t *satc_polygon_set_offset(satc_polygon_t *polygon, double *offset);
satc_polygon_t *satc_polygon_rotate(satc_polygon_t *polygon, double angle);
satc_polygon_t *satc_polygon_translate(satc_polygon_t *polygon, double x, double y);

// Get polygon properties
satc_polygon_t *satc_polygon_get_aabb(satc_polygon_t *polygon);
double *satc_polygon_get_centroid(satc_polygon_t *polygon);
```

#### Boxes
```c
// Create box
satc_box_t *satc_box_create(double *pos, double w, double h);

// Destroy box
void satc_box_destroy(satc_box_t *box);

// Convert to polygon
satc_polygon_t *satc_box_to_polygon(satc_box_t *box);
```

#### Response
```c
// Create response object
satc_response_t *satc_response_create();

// Destroy response (does NOT destroy a and b shapes)
void satc_response_destroy(satc_response_t *response);
```

### Collision Detection

#### Point Tests
```c
// Check if point is inside circle
bool satc_point_in_circle(double *point, satc_circle_t *circle);

// Check if point is inside polygon
bool satc_point_in_polygon(double *point, satc_polygon_t *polygon);
```

#### Shape vs Shape Tests
```c
// Circle vs Circle
bool satc_test_circle_circle(satc_circle_t *a, satc_circle_t *b, satc_response_t *response);

// Polygon vs Circle
bool satc_test_polygon_circle(satc_polygon_t *polygon, satc_circle_t *circle, satc_response_t *response);

// Circle vs Polygon
bool satc_test_circle_polygon(satc_circle_t *circle, satc_polygon_t *polygon, satc_response_t *response);

// Polygon vs Polygon
bool satc_test_polygon_polygon(satc_polygon_t *a, satc_polygon_t *b, satc_response_t *response);
```

### Utility Functions

```c
// Check if two point sets represent a separating axis
bool satc_is_separating_axis(double *a_pos, double *b_pos,
                              size_t a_len, double **a_points,
                              size_t b_len, double **b_points,
                              double *axis, satc_response_t *response);

// Determine voronoi region for collision detection
int satc_voronoi_region(double *line, double *point);

// Project points onto axis
void satc_flatten_points_on(size_t len, double **points, double *normal, double *result);
```

## Usage Example

```c
#include "satc.h"

int main(void)
{
    // Create positions
    satc_point_alloca_xy(pos1, 100.0, 100.0);
    satc_point_alloca_xy(pos2, 120.0, 110.0);

    // Create two circles
    satc_circle_t *circle1 = satc_circle_create(pos1, 50.0);
    satc_circle_t *circle2 = satc_circle_create(pos2, 40.0);

    // Create response object
    satc_response_t *response = satc_response_create();

    // Test collision
    if (satc_test_circle_circle(circle1, circle2, response)) {
        // Collision detected!
        double overlap = response->overlap;
        double sep_x = satc_point_get_x(response->overlap_v);
        double sep_y = satc_point_get_y(response->overlap_v);

        // Move circle1 out of collision
        satc_point_get_x(circle1->pos) -= sep_x;
        satc_point_get_y(circle1->pos) -= sep_y;
    }

    // Cleanup
    satc_response_destroy(response);
    satc_circle_destroy(circle1);
    satc_circle_destroy(circle2);

    return 0;
}
```

### Polygon Collision Example

```c
#include "satc.h"

int main(void)
{
    // Create triangle
    satc_point_alloca_xy(tri_pos, 200.0, 200.0);
    double **tri_points = (double**)alloca(sizeof(double*) * 3);

    satc_point_alloca_xy(p0, 0.0, -30.0);
    satc_point_alloca_xy(p1, 25.0, 20.0);
    satc_point_alloca_xy(p2, -25.0, 20.0);
    tri_points[0] = p0;
    tri_points[1] = p1;
    tri_points[2] = p2;

    satc_polygon_t *triangle = satc_polygon_create(tri_pos, 3, tri_points);

    // Create circle
    satc_point_alloca_xy(circ_pos, 220.0, 210.0);
    satc_circle_t *circle = satc_circle_create(circ_pos, 25.0);

    // Test collision
    satc_response_t *response = satc_response_create();

    if (satc_test_polygon_circle(triangle, circle, response)) {
        // Handle collision
        // response->overlap_v contains separation vector
    }

    // Cleanup
    satc_response_destroy(response);
    satc_polygon_destroy(triangle);
    satc_circle_destroy(circle);

    return 0;
}
```

## VOS/TCC Compatibility Notes

### Dependencies
The library requires these standard headers:
- `stdio.h`
- `stdlib.h`
- `stdbool.h`
- `math.h`
- `float.h`

### Memory Allocation
- Uses `alloca()` for stack-based temporary allocations (fast, auto-cleanup)
- Uses `malloc()/free()` for heap allocations
- TCC supports both `alloca` and standard heap allocation

### Important Considerations
- All shapes use double-precision floats for accuracy
- Points are represented as `double*` arrays (not structs)
- Stack-allocated points (via `satc_*_alloca` macros) are automatically freed when leaving scope
- Heap-allocated points must be manually freed with `satc_point_destroy()`

### Compilation Notes
- The library is header-only; just include `satc.h`
- No special defines needed
- Ensure math library is linked if required by your compiler

### Performance Tips
- Use stack allocation (`satc_point_alloca`) for temporary points in hot loops
- Prefer `satc_point_len2()` over `satc_point_len()` when comparing distances (avoids sqrt)
- Cache polygon AABBs for broad-phase collision detection
- Use `satc_box_to_polygon()` to create optimized rectangles

### Known Limitations
- Only handles convex polygons
- Not a physics engine (no velocity, forces, friction)
- Double precision may be overkill for simple games (but provides accuracy)
