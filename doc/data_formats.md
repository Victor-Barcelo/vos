# Data Format Libraries for VOS

Single-file C libraries for parsing and writing various file formats.

---

## Image Formats

### stb_image.h - Image Loading
- **URL:** https://github.com/nothings/stb
- **License:** Public Domain
- **Formats:** JPEG, PNG, BMP, GIF, TGA, PSD, HDR, PIC, PNM

```c
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int width, height, channels;
unsigned char* pixels = stbi_load("image.png", &width, &height, &channels, 0);
if (pixels) {
    // Use pixel data (RGBA if channels == 4)
    // pixels[y * width * channels + x * channels + c]
    stbi_image_free(pixels);
}

// Load from memory
unsigned char* pixels = stbi_load_from_memory(buffer, buffer_len, &w, &h, &c, 0);
```

### stb_image_write.h - Image Writing
- **URL:** https://github.com/nothings/stb
- **License:** Public Domain
- **Formats:** PNG, BMP, TGA, JPG

```c
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Write PNG
stbi_write_png("output.png", width, height, channels, pixels, width * channels);

// Write BMP
stbi_write_bmp("output.bmp", width, height, channels, pixels);

// Write TGA
stbi_write_tga("output.tga", width, height, channels, pixels);

// Write JPG (quality 1-100)
stbi_write_jpg("output.jpg", width, height, channels, pixels, 90);
```

### QOI - Quite OK Image Format
- **URL:** https://github.com/phoboslab/qoi
- **License:** MIT
- **Features:** Lossless, fast encode/decode, simple format

```c
#define QOI_IMPLEMENTATION
#include "qoi.h"

// Encode
qoi_desc desc = {width, height, 4, QOI_SRGB};
int out_len;
void* encoded = qoi_encode(pixels, &desc, &out_len);
write_file("image.qoi", encoded, out_len);
free(encoded);

// Decode
qoi_desc desc;
void* pixels = qoi_decode(file_data, file_len, &desc, 4);
// desc.width, desc.height, desc.channels
free(pixels);
```

### upng - Minimal PNG Decoder
- **URL:** https://github.com/elanthis/upng
- **License:** Public Domain
- **Size:** ~1500 lines

```c
#include "upng.h"

upng_t* upng = upng_new_from_file("image.png");
upng_decode(upng);

if (upng_get_error(upng) == UPNG_EOK) {
    unsigned width = upng_get_width(upng);
    unsigned height = upng_get_height(upng);
    const unsigned char* buffer = upng_get_buffer(upng);
}

upng_free(upng);
```

### TinyJPEG - JPEG Decoder
- **URL:** https://github.com/nicktasios/TinyJPEG
- **License:** Public Domain

### Bitmap Loading (DIY - BMP Only)
```c
#pragma pack(push, 1)
typedef struct {
    uint16_t type;        // "BM" = 0x4D42
    uint32_t size;
    uint16_t reserved1, reserved2;
    uint32_t offset;
} BMPHeader;

typedef struct {
    uint32_t size;
    int32_t width, height;
    uint16_t planes, bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_ppm, y_ppm;
    uint32_t colors_used, colors_important;
} BMPInfoHeader;
#pragma pack(pop)

uint8_t* load_bmp(const char* path, int* w, int* h) {
    FILE* f = fopen(path, "rb");
    BMPHeader header;
    BMPInfoHeader info;
    fread(&header, sizeof(header), 1, f);
    fread(&info, sizeof(info), 1, f);

    if (header.type != 0x4D42 || info.bpp != 24)
        return NULL;

    *w = info.width;
    *h = abs(info.height);

    int row_size = ((*w * 3 + 3) & ~3);  // Padded to 4 bytes
    uint8_t* pixels = malloc(*w * *h * 3);

    fseek(f, header.offset, SEEK_SET);
    for (int y = *h - 1; y >= 0; y--) {  // BMP is bottom-up
        fread(pixels + y * *w * 3, *w * 3, 1, f);
        fseek(f, row_size - *w * 3, SEEK_CUR);  // Skip padding
        // Swap BGR to RGB
        for (int x = 0; x < *w; x++) {
            uint8_t* p = pixels + (y * *w + x) * 3;
            uint8_t t = p[0]; p[0] = p[2]; p[2] = t;
        }
    }
    fclose(f);
    return pixels;
}
```

---

## Audio Formats

### dr_wav - WAV Loading
- **URL:** https://github.com/mackron/dr_libs
- **License:** Public Domain

```c
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

unsigned int channels, sampleRate;
drwav_uint64 totalPCMFrameCount;
int16_t* samples = drwav_open_file_and_read_pcm_frames_s16(
    "sound.wav", &channels, &sampleRate, &totalPCMFrameCount, NULL);

if (samples) {
    // Use samples[frame * channels + channel]
    drwav_free(samples, NULL);
}
```

### dr_mp3 - MP3 Decoding
- **URL:** https://github.com/mackron/dr_libs
- **License:** Public Domain

```c
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

drmp3_config config;
drmp3_uint64 totalPCMFrameCount;
int16_t* samples = drmp3_open_file_and_read_pcm_frames_s16(
    "music.mp3", &config, &totalPCMFrameCount, NULL);

if (samples) {
    // config.channels, config.sampleRate
    drmp3_free(samples, NULL);
}
```

### dr_flac - FLAC Decoding
- **URL:** https://github.com/mackron/dr_libs
- **License:** Public Domain

```c
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

unsigned int channels, sampleRate;
drflac_uint64 totalPCMFrameCount;
int32_t* samples = drflac_open_file_and_read_pcm_frames_s32(
    "music.flac", &channels, &sampleRate, &totalPCMFrameCount, NULL);
```

### stb_vorbis - OGG Vorbis Decoding
- **URL:** https://github.com/nothings/stb
- **License:** Public Domain

```c
#include "stb_vorbis.c"

int channels, sample_rate;
short* samples;
int num_samples = stb_vorbis_decode_filename(
    "music.ogg", &channels, &sample_rate, &samples);

if (num_samples > 0) {
    // Use samples
    free(samples);
}
```

### TinySoundFont - SoundFont/MIDI
- **URL:** https://github.com/schellingb/TinySoundFont
- **License:** MIT
- **Features:** Load SF2 files, render MIDI

---

## Font Formats

### stb_truetype.h - TrueType Loading
- **URL:** https://github.com/nothings/stb
- **License:** Public Domain

```c
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// Load font file
unsigned char* ttf_buffer = load_file("font.ttf");

stbtt_fontinfo font;
stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0));

// Render character to bitmap
float scale = stbtt_ScaleForPixelHeight(&font, 24);  // 24px
int w, h, xoff, yoff;
unsigned char* bitmap = stbtt_GetCodepointBitmap(
    &font, 0, scale, 'A', &w, &h, &xoff, &yoff);

// bitmap is a w*h grayscale image
stbtt_FreeBitmap(bitmap, NULL);
```

### schrift - Minimal TrueType
- **URL:** https://github.com/tomolt/libschrift
- **License:** ISC
- **Features:** Smaller than stb_truetype, basic rendering

### BMFont Loader (DIY)
For bitmap fonts exported from tools like BMFont:
```c
typedef struct {
    int id;
    int x, y, w, h;
    int xoffset, yoffset;
    int xadvance;
} BitmapChar;

typedef struct {
    BitmapChar chars[256];
    int line_height;
    int base;
} BitmapFont;

// Parse .fnt file (text format)
void load_bmfont(BitmapFont* font, const char* path) {
    FILE* f = fopen(path, "r");
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "common ", 7) == 0) {
            sscanf(line, "common lineHeight=%d base=%d",
                   &font->line_height, &font->base);
        } else if (strncmp(line, "char ", 5) == 0) {
            BitmapChar c;
            sscanf(line, "char id=%d x=%d y=%d width=%d height=%d "
                   "xoffset=%d yoffset=%d xadvance=%d",
                   &c.id, &c.x, &c.y, &c.w, &c.h,
                   &c.xoffset, &c.yoffset, &c.xadvance);
            if (c.id < 256) font->chars[c.id] = c;
        }
    }
    fclose(f);
}
```

---

## 3D Model Formats

### tinyobjloader-c - OBJ Loading
- **URL:** https://github.com/syoyo/tinyobjloader-c
- **License:** MIT

```c
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c.h"

tinyobj_attrib_t attrib;
tinyobj_shape_t* shapes = NULL;
size_t num_shapes;
tinyobj_material_t* materials = NULL;
size_t num_materials;

int ret = tinyobj_parse_obj(&attrib, &shapes, &num_shapes,
                            &materials, &num_materials,
                            "model.obj", load_file_callback, NULL, 0);

if (ret == TINYOBJ_SUCCESS) {
    // attrib.vertices[i * 3 + 0/1/2] = x/y/z
    // attrib.normals[i * 3 + 0/1/2]
    // attrib.texcoords[i * 2 + 0/1]

    for (size_t s = 0; s < num_shapes; s++) {
        // shapes[s].face_offset, shapes[s].length
    }

    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);
}
```

### cgltf - glTF 2.0 Loading
- **URL:** https://github.com/jkuhlmann/cgltf
- **License:** MIT

```c
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

cgltf_options options = {0};
cgltf_data* data = NULL;
cgltf_result result = cgltf_parse_file(&options, "model.gltf", &data);

if (result == cgltf_result_success) {
    cgltf_load_buffers(&options, data, "model.gltf");

    for (size_t i = 0; i < data->meshes_count; i++) {
        cgltf_mesh* mesh = &data->meshes[i];
        // Process mesh primitives
    }

    cgltf_free(data);
}
```

### Simple OBJ Loader (DIY)
```c
typedef struct {
    float* vertices;   // x, y, z
    float* texcoords;  // u, v
    float* normals;    // nx, ny, nz
    int* indices;      // triangle indices
    int vertex_count;
    int index_count;
} Mesh;

Mesh* load_obj(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    // First pass: count elements
    int v_count = 0, vt_count = 0, vn_count = 0, f_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') v_count++;
        else if (line[0] == 'v' && line[1] == 't') vt_count++;
        else if (line[0] == 'v' && line[1] == 'n') vn_count++;
        else if (line[0] == 'f') f_count++;
    }

    // Allocate
    float* v = malloc(v_count * 3 * sizeof(float));
    float* vt = malloc(vt_count * 2 * sizeof(float));
    float* vn = malloc(vn_count * 3 * sizeof(float));

    // Second pass: read data
    rewind(f);
    int vi = 0, vti = 0, vni = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            sscanf(line, "v %f %f %f", &v[vi*3], &v[vi*3+1], &v[vi*3+2]);
            vi++;
        } else if (line[0] == 'v' && line[1] == 't') {
            sscanf(line, "vt %f %f", &vt[vti*2], &vt[vti*2+1]);
            vti++;
        } else if (line[0] == 'v' && line[1] == 'n') {
            sscanf(line, "vn %f %f %f", &vn[vni*3], &vn[vni*3+1], &vn[vni*3+2]);
            vni++;
        }
    }

    // Build final mesh (simplified - assumes v/vt/vn format)
    // ... face parsing and vertex deduplication ...

    fclose(f);
    return mesh;
}
```

---

## Archive Formats

### miniz - ZIP Archives
- **URL:** https://github.com/richgel999/miniz
- **License:** MIT

```c
#define MINIZ_IMPLEMENTATION
#include "miniz.h"

// Read ZIP
mz_zip_archive zip;
memset(&zip, 0, sizeof(zip));
mz_zip_reader_init_file(&zip, "archive.zip", 0);

int num_files = mz_zip_reader_get_num_files(&zip);
for (int i = 0; i < num_files; i++) {
    mz_zip_archive_file_stat stat;
    mz_zip_reader_file_stat(&zip, i, &stat);
    printf("File: %s (%llu bytes)\n", stat.m_filename, stat.m_uncomp_size);

    // Extract to memory
    size_t size;
    void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
    if (data) {
        // Use data
        free(data);
    }
}
mz_zip_reader_end(&zip);

// Create ZIP
mz_zip_archive zip_w;
memset(&zip_w, 0, sizeof(zip_w));
mz_zip_writer_init_file(&zip_w, "new.zip", 0);
mz_zip_writer_add_mem(&zip_w, "hello.txt", "Hello World", 11, MZ_DEFAULT_COMPRESSION);
mz_zip_writer_finalize_archive(&zip_w);
mz_zip_writer_end(&zip_w);
```

### microtar - TAR Archives
- **URL:** https://github.com/rxi/microtar
- **License:** MIT

```c
#include "microtar.h"

// Read TAR
mtar_t tar;
mtar_header_t h;
mtar_open(&tar, "archive.tar", "r");

while (mtar_read_header(&tar, &h) != MTAR_ENULLRECORD) {
    printf("File: %s (%d bytes)\n", h.name, h.size);

    char* data = malloc(h.size);
    mtar_read_data(&tar, data, h.size);
    // Use data
    free(data);

    mtar_next(&tar);
}

mtar_close(&tar);

// Write TAR
mtar_open(&tar, "new.tar", "w");
mtar_write_file_header(&tar, "hello.txt", 11);
mtar_write_data(&tar, "Hello World", 11);
mtar_finalize(&tar);
mtar_close(&tar);
```

---

## Configuration Formats

### cJSON - JSON
Already covered in system_libraries.md

### minIni - INI Files
Already covered in system_libraries.md

### tomlc99 - TOML
- **URL:** https://github.com/cktan/tomlc99
- **License:** MIT

```c
#include "toml.h"

FILE* fp = fopen("config.toml", "r");
char errbuf[200];
toml_table_t* conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
fclose(fp);

if (conf) {
    toml_table_t* server = toml_table_in(conf, "server");
    toml_datum_t host = toml_string_in(server, "host");
    toml_datum_t port = toml_int_in(server, "port");

    printf("Host: %s, Port: %lld\n", host.u.s, port.u.i);

    free(host.u.s);
    toml_free(conf);
}
```

### csv.h - CSV Parsing
- **URL:** https://github.com/semitrivial/csv_parser
- **License:** MIT

### Simple CSV Parser (DIY)
```c
typedef struct {
    char** fields;
    int field_count;
} CSVRow;

CSVRow* csv_parse_line(const char* line) {
    CSVRow* row = malloc(sizeof(CSVRow));
    row->fields = NULL;
    row->field_count = 0;

    const char* p = line;
    while (*p) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t') p++;

        char field[256];
        int fi = 0;

        if (*p == '"') {
            p++;  // Skip opening quote
            while (*p && !(*p == '"' && *(p+1) != '"')) {
                if (*p == '"' && *(p+1) == '"') p++;  // Escaped quote
                field[fi++] = *p++;
            }
            if (*p == '"') p++;
        } else {
            while (*p && *p != ',' && *p != '\n' && *p != '\r') {
                field[fi++] = *p++;
            }
        }
        field[fi] = '\0';

        row->fields = realloc(row->fields, (row->field_count + 1) * sizeof(char*));
        row->fields[row->field_count++] = strdup(field);

        if (*p == ',') p++;
        else break;
    }

    return row;
}
```

---

## Map/Level Formats

### cute_tiled.h - Tiled Map Editor
- **URL:** https://github.com/RandyGaul/cute_headers
- **License:** Public Domain
- **Features:** Load .tmx and .tsx files from Tiled

```c
#define CUTE_TILED_IMPLEMENTATION
#include "cute_tiled.h"

cute_tiled_map_t* map = cute_tiled_load_map_from_file("level.tmx", NULL);

printf("Map: %dx%d tiles, %dx%d pixels\n",
       map->width, map->height, map->tilewidth, map->tileheight);

cute_tiled_layer_t* layer = map->layers;
while (layer) {
    if (layer->data) {
        for (int y = 0; y < map->height; y++) {
            for (int x = 0; x < map->width; x++) {
                int tile_id = layer->data[y * map->width + x];
                // Render tile
            }
        }
    }
    layer = layer->next;
}

cute_tiled_free_map(map);
```

### LDTK Loader
- **URL:** https://ldtk.io/
- **Format:** JSON-based, can use cJSON to parse

---

## Recommended Format Library Bundle

```
/usr/include/formats/
├── images/
│   ├── stb_image.h        # Load PNG, JPG, BMP, etc.
│   ├── stb_image_write.h  # Save PNG, BMP, TGA, JPG
│   └── qoi.h              # Fast lossless image format
├── audio/
│   ├── dr_wav.h           # WAV loading
│   ├── dr_mp3.h           # MP3 decoding
│   └── stb_vorbis.c       # OGG Vorbis
├── fonts/
│   └── stb_truetype.h     # TrueType fonts
├── models/
│   ├── tinyobj_loader_c.h # OBJ models
│   └── cgltf.h            # glTF 2.0
├── archives/
│   ├── miniz.h            # ZIP
│   └── microtar.h         # TAR
└── maps/
    └── cute_tiled.h       # Tiled editor maps
```

## See Also

- [game_resources.md](game_resources.md) - Game development libraries
- [system_libraries.md](system_libraries.md) - System utilities
