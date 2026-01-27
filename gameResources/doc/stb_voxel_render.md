# stb_voxel_render.h

GPU-based voxel/block world mesh generator with GLSL shaders.

## Source

- **Repository**: [https://github.com/nothings/stb](https://github.com/nothings/stb)
- **Direct Link**: [https://github.com/nothings/stb/blob/master/stb_voxel_render.h](https://github.com/nothings/stb/blob/master/stb_voxel_render.h)
- **Version**: 0.89
- **Author**: Sean Barrett

## License

Public domain / MIT dual license. You can choose whichever you prefer.

## Description

stb_voxel_render helps render large-scale "voxel" worlds for games, specifically with blocks that can have textures and can also be a few shapes other than cubes. It works by creating triangle meshes from voxel data.

The library includes:
- Converter from dense 3D arrays of block info to vertex mesh
- Vertex & fragment shaders (GLSL)
- Assistance in setting up shader state

## Features

### Block Features
- Textured blocks with two texture layers per face
- Colored voxels with 2^24 colors (untextured mode)
- Block shapes: full cubes, half-height cubes, diagonal slopes, half-height diagonals
- More complex shapes for continuous terrain ("vheight" system)
- Per-texture scaling
- 256 textures per texture array

### Shader Features
- Vertices aligned on integer lattice, Z on multiples of 0.5
- Per-vertex lighting/ambient occlusion (6 bits)
- Per-vertex texture crossfade (3 bits)
- Per-face texture IDs (8-bit index into texture arrays)
- Per-face color (6-bit palette index)
- Per-face normal (5-bit for lighting & texture coordinates)
- Per-face texture rotation (2-bit)
- Multiple blend modes: alpha composite or modulate/multiply
- Half-lambert directional + constant ambient lighting
- Fullbright/emissive faces support
- Installable custom lighting and fog functions

### Performance
- Compact vertex format: 20 bytes per quad (mode 1)
- Optional 32 bytes per quad (mode 0, easier to set up)
- Frustum culling support via bounding box queries
- Multiple output meshes (e.g., separate opaque and transparent)

## Configuration Modes

```c
#define STBVOX_CONFIG_MODE <integer>  // REQUIRED

// Mode values:
//   0  - Textured blocks, 32-byte quads (easiest to get working)
//   1  - Textured blocks, 20-byte quads (uses texture buffer)
//  20  - Untextured blocks, 32-byte quads
//  21  - Untextured blocks, 20-byte quads
```

### Optional Configuration

```c
#define STBVOX_CONFIG_PRECISION_Z  <0|1>      // Z fractional bits (default 1)
#define STBVOX_CONFIG_BLOCKTYPE_SHORT         // Use 16-bit blocktypes
#define STBVOX_CONFIG_OPENGL_MODELVIEW        // Use gl_ModelView matrix
#define STBVOX_CONFIG_PREFER_TEXBUFFER        // Use texture buffers for uniforms
#define STBVOX_CONFIG_LIGHTING_SIMPLE         // Simple point light + ambient
#define STBVOX_CONFIG_LIGHTING                // Custom lighting function hook
#define STBVOX_CONFIG_FOG_SMOOTHSTEP          // Simple fog system
#define STBVOX_CONFIG_FOG                     // Custom fog function hook
#define STBVOX_CONFIG_DISABLE_TEX2            // Disable texture #2 processing
#define STBVOX_CONFIG_TEX1_EDGE_CLAMP         // Edge clamp texture #1
#define STBVOX_CONFIG_TEX2_EDGE_CLAMP         // Edge clamp texture #2
#define STBVOX_CONFIG_ROTATION_IN_LIGHTING    // Store rotation in lighting
#define STBVOX_CONFIG_VHEIGHT_IN_LIGHTING     // Store vheight in lighting
#define STBVOX_CONFIG_PREMULTIPLIED_ALPHA     // Use premultiplied alpha
```

## API Reference

### Types

```c
typedef struct stbvox_mesh_maker stbvox_mesh_maker;
typedef struct stbvox_input_description stbvox_input_description;
typedef struct stbvox_uniform_info stbvox_uniform_info;

// Block type (unsigned char or unsigned short)
typedef unsigned char stbvox_block_type;  // or short with CONFIG_BLOCKTYPE_SHORT

// 24-bit color
typedef struct {
   unsigned char r, g, b;
} stbvox_rgb;

// Geometry types
enum {
   STBVOX_GEOM_empty,
   STBVOX_GEOM_knockout,     // Creates a hole in the mesh
   STBVOX_GEOM_solid,
   STBVOX_GEOM_transp,       // Solid but transparent contents
   STBVOX_GEOM_slab_upper,
   STBVOX_GEOM_slab_lower,
   STBVOX_GEOM_floor_slope_north_is_top,
   STBVOX_GEOM_ceil_slope_north_is_bottom,
   STBVOX_GEOM_crossed_pair, // Corner-to-corner pairs
   STBVOX_GEOM_force,        // Always visible faces
   STBVOX_GEOM_floor_vheight_03,  // Variable height floors
   STBVOX_GEOM_floor_vheight_12,
   STBVOX_GEOM_ceil_vheight_03,
   STBVOX_GEOM_ceil_vheight_12,
   STBVOX_GEOM_count,
};

// Face directions
enum {
   STBVOX_FACE_east,   // +X
   STBVOX_FACE_north,  // +Y
   STBVOX_FACE_west,   // -X
   STBVOX_FACE_south,  // -Y
   STBVOX_FACE_up,     // +Z
   STBVOX_FACE_down,   // -Z
   STBVOX_FACE_count,
};

// Uniform types
enum {
   STBVOX_UNIFORM_face_data,    // Sampler for face texture buffer
   STBVOX_UNIFORM_transform,    // Transform data (changes per-mesh)
   STBVOX_UNIFORM_tex_array,    // Two texture array samplers
   STBVOX_UNIFORM_texscale,     // Texture scaling table
   STBVOX_UNIFORM_color_table,  // 64-color palette
   STBVOX_UNIFORM_normals,      // Normal table (internal)
   STBVOX_UNIFORM_texgen,       // Texture gen table (internal)
   STBVOX_UNIFORM_ambient,      // Lighting & fog info
   STBVOX_UNIFORM_camera_pos,   // Camera position
   STBVOX_UNIFORM_count,
};
```

### Mesh Creation Functions

```c
void stbvox_init_mesh_maker(stbvox_mesh_maker *mm);
// Initialize a mesh-maker context (one per thread)

void stbvox_set_buffer(stbvox_mesh_maker *mm, int mesh, int slot,
                       void *buffer, size_t len);
// Set output buffer for mesh generation
// mesh: which mesh (for opaque/transparent separation)
// slot: buffer slot (depends on mode)

int stbvox_get_buffer_count(stbvox_mesh_maker *mm);
// Get number of buffers needed per mesh

int stbvox_get_buffer_size_per_quad(stbvox_mesh_maker *mm, int slot);
// Get bytes used per quad for a slot

void stbvox_set_default_mesh(stbvox_mesh_maker *mm, int mesh);
// Set default output mesh when no selector specified

stbvox_input_description *stbvox_get_input_description(stbvox_mesh_maker *mm);
// Get pointer to input description to fill out

void stbvox_set_input_stride(stbvox_mesh_maker *mm,
                              int x_stride_in_elements,
                              int y_stride_in_elements);
// Set stride between 3D array elements (Z always consecutive)

void stbvox_set_input_range(stbvox_mesh_maker *mm,
                            int x0, int y0, int z0,
                            int x1, int y1, int z1);
// Set range to process (lower inclusive, upper exclusive)
// Note: Accesses array elements 1 beyond limits

int stbvox_make_mesh(stbvox_mesh_maker *mm);
// Generate mesh data. Returns 1 on success, 0 if buffer full
// On 0, switch buffers and call again

int stbvox_get_quad_count(stbvox_mesh_maker *mm, int mesh);
// Get number of quads generated

void stbvox_set_mesh_coordinates(stbvox_mesh_maker *mm, int x, int y, int z);
// Set global coordinates for the chunk origin

void stbvox_get_bounds(stbvox_mesh_maker *mm, float bounds[2][3]);
// Get mesh bounds in global coordinates (for frustum culling)

void stbvox_get_transform(stbvox_mesh_maker *mm, float transform[3][3]);
// Get transform data for shader uniform

void stbvox_reset_buffers(stbvox_mesh_maker *mm);
// Reset buffers for reuse after copying data out
```

### Shader Functions

```c
char *stbvox_get_vertex_shader(void);
// Get GLSL vertex shader source

char *stbvox_get_fragment_shader(void);
// Get GLSL fragment shader source

char *stbvox_get_fragment_shader_alpha_only(void);
// Get fragment shader for depth-only pass (alpha test)

int stbvox_get_uniform_info(stbvox_uniform_info *info, int uniform);
// Get information about a uniform for setup
```

### Input Description Structure

```c
struct stbvox_input_description {
   unsigned char lighting_at_vertices;  // Lighting mode flag

   // 3D map arrays (indexed by x*x_stride + y*y_stride + z)
   stbvox_rgb *rgb;           // 24-bit voxel color (modes 20/21)
   unsigned char *lighting;    // Lighting/AO values
   stbvox_block_type *blocktype;  // Block type index
   unsigned char *geometry;    // Geometry type + rotation
   unsigned char *tex2;        // Per-voxel texture #2
   unsigned char *color;       // Per-voxel color
   unsigned char *selector;    // Output mesh selector
   unsigned char *overlay;     // Overlay index
   unsigned char *side_texrot; // Side face texture rotation
   unsigned char *rotate;      // Block/overlay rotation

   // Palette arrays (indexed by blocktype)
   unsigned char *block_geometry;     // Geometry per blocktype
   unsigned char *block_tex1;         // Texture #1 per blocktype
   unsigned char (*block_tex1_face)[6];  // Texture #1 per face
   unsigned char *block_tex2;         // Texture #2 per blocktype
   unsigned char (*block_tex2_face)[6];  // Texture #2 per face
   unsigned char *block_color;        // Color per blocktype
   unsigned char (*block_color_face)[6]; // Color per face
   unsigned char *block_texlerp;      // Texture blend per blocktype
   unsigned char (*block_texlerp_face)[6];
   unsigned char *block_vheight;      // Variable height per blocktype
   unsigned char *block_selector;     // Mesh selector per blocktype
   unsigned char *block_side_texrot;  // Side texrot per blocktype

   // Overlay arrays (indexed by overlay value)
   unsigned char (*overlay_tex1)[6];
   unsigned char (*overlay_tex2)[6];
   unsigned char (*overlay_color)[6];
   unsigned char *overlay_side_texrot;

   // Correlation arrays
   unsigned char *tex2_for_tex1;  // Tex2 indexed by tex1
};
```

### Encoding Macros

```c
STBVOX_MAKE_GEOMETRY(geom, rot, vheight)
STBVOX_MAKE_LIGHTING(lighting)
STBVOX_MAKE_LIGHTING_EXT(lighting, rot_or_vheight)
STBVOX_MAKE_COLOR(color_number, tex1_enable, tex2_enable)
STBVOX_MAKE_VHEIGHT(sw, se, nw, ne)
STBVOX_MAKE_MATROT(block, overlay, ecolor)
STBVOX_MAKE_SIDE_TEXROT(rot_e, rot_n, rot_w, rot_s)

#define STBVOX_COLOR_TEX1_ENABLE   64
#define STBVOX_COLOR_TEX2_ENABLE  128
```

## Usage Example

```c
#define STBVOX_CONFIG_MODE 0
#define STB_VOXEL_RENDER_IMPLEMENTATION
#include "stb_voxel_render.h"

// Setup mesh maker
stbvox_mesh_maker mm;
stbvox_init_mesh_maker(&mm);

// Allocate output buffers
void *vertex_buffer = malloc(1024 * 1024);
stbvox_set_buffer(&mm, 0, 0, vertex_buffer, 1024 * 1024);

// Setup input data
stbvox_input_description *input = stbvox_get_input_description(&mm);
memset(input, 0, sizeof(*input));

// Your voxel data (34x34x34 for a 32x32x32 chunk with borders)
unsigned char blocktype[34][34][34];
unsigned char block_geometry[256];
unsigned char block_tex1[256];

// Fill input
input->blocktype = &blocktype[0][0][0];
input->block_geometry = block_geometry;
input->block_tex1 = block_tex1;

// Set block types
block_geometry[0] = STBVOX_GEOM_empty;
block_geometry[1] = STBVOX_GEOM_solid;
block_tex1[1] = 0;  // Texture index

// Configure mesher
stbvox_set_input_stride(&mm, 34*34, 34);  // Z consecutive
stbvox_set_input_range(&mm, 1, 1, 1, 33, 33, 33);
stbvox_set_mesh_coordinates(&mm, chunk_x * 32, chunk_y * 32, chunk_z * 32);

// Generate mesh
while (!stbvox_make_mesh(&mm)) {
    // Buffer full - copy data out and reset
    int quads = stbvox_get_quad_count(&mm, 0);
    upload_to_gpu(vertex_buffer, quads * 32);
    stbvox_reset_buffers(&mm);
    stbvox_set_buffer(&mm, 0, 0, vertex_buffer, 1024 * 1024);
}

// Final upload
int quads = stbvox_get_quad_count(&mm, 0);
upload_to_gpu(vertex_buffer, quads * 32);

// Get transform for rendering
float transform[3][3];
stbvox_get_transform(&mm, transform);

// Compile shaders
const char *vs = stbvox_get_vertex_shader();
const char *fs = stbvox_get_fragment_shader();
// ... compile and link ...
```

## VOS/TCC Compatibility Notes

### Compatibility Issues

1. **GLSL Shaders**: The library generates GLSL shaders. VOS would need an OpenGL implementation or shader translation layer.

2. **Floating-Point Heavy**: Extensive floating-point math for transforms and shader uniforms.

3. **Memory Requirements**: Vertex buffers can be large (20-32 bytes per quad, potentially millions of quads).

4. **No Direct Graphics API**: The library doesn't call OpenGL directly, but requires you to implement the GPU interface.

### Potential VOS Usage

For VOS, consider:

1. **Software Rasterization**: Use the mesh generation but implement a software renderer instead of GPU shaders.

2. **Reduced Resolution**: Generate meshes for smaller chunks and lower detail.

3. **Pre-computed Meshes**: Generate meshes offline and load pre-built vertex data.

4. **Alternative Rendering**: Extract face data and render using a simpler 2D projection system.

### Simplified Integration

```c
// Use the mesh generation for face extraction only
stbvox_mesh_maker mm;
stbvox_init_mesh_maker(&mm);

// Generate mesh
// ...

// Extract quad count and manually render faces
int quads = stbvox_get_quad_count(&mm, 0);
// Parse vertex buffer and render with software rasterizer
```

### Resource Considerations

- Vertex buffer: ~20MB for 1M quads (mode 1)
- Input arrays: ~1MB for 256x256x128 world
- Palette arrays: ~2KB per blocktype set

For memory-constrained VOS environments, consider chunking the world into 16x16x16 or smaller sections.
