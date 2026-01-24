#ifndef VOS_SMALL3D_H
#define VOS_SMALL3D_H

// Compatibility wrapper. The upstream project is named "small3dlib" and ships
// as a single header file. VOS installs both `small3d.h` and `small3dlib.h` so
// you can use either include style.
//
// Usage (in exactly one .c file):
//   #define S3L_PIXEL_FUNCTION my_pixel
//   #define S3L_RESOLUTION_X 640
//   #define S3L_RESOLUTION_Y 480
//   #define S3L_Z_BUFFER 1
//   #include <small3d.h>
//
// Your `S3L_PIXEL_FUNCTION` will be called for each rasterized pixel.
#include "small3dlib.h"

#endif

