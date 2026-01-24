# System & Utility Libraries for VOS

Single-file C libraries for system utilities, data structures, and general programming.

---

## Data Structures

### stb_ds.h - Dynamic Arrays & Hash Maps
- **URL:** https://github.com/nothings/stb
- **License:** Public Domain
- **Features:** Dynamic arrays, string hash maps, integer hash maps

```c
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// Dynamic array
int* arr = NULL;
arrput(arr, 1);
arrput(arr, 2);
arrput(arr, 3);
for (int i = 0; i < arrlen(arr); i++)
    printf("%d ", arr[i]);
arrfree(arr);

// String hash map
struct { char* key; int value; }* map = NULL;
shput(map, "one", 1);
shput(map, "two", 2);
int v = shget(map, "one");  // 1
shfree(map);
```

### uthash.h - Hash Tables
- **URL:** https://troydhanson.github.io/uthash/
- **License:** BSD
- **Features:** Add hash table to any struct

```c
#include "uthash.h"

typedef struct {
    int id;
    char name[32];
    UT_hash_handle hh;
} User;

User* users = NULL;

void add_user(int id, const char* name) {
    User* u = malloc(sizeof(User));
    u->id = id;
    strcpy(u->name, name);
    HASH_ADD_INT(users, id, u);
}

User* find_user(int id) {
    User* u;
    HASH_FIND_INT(users, &id, u);
    return u;
}
```

### vec.h - Simple Dynamic Array
- **URL:** https://github.com/rxi/vec
- **License:** MIT
- **Size:** ~200 lines

```c
#include "vec.h"

vec_int_t v;
vec_init(&v);
vec_push(&v, 10);
vec_push(&v, 20);
printf("%d\n", v.data[0]);
vec_deinit(&v);
```

### queue.h / list.h (BSD)
Standard BSD queue and list macros - already in many systems:

```c
#include <sys/queue.h>

struct entry {
    int value;
    TAILQ_ENTRY(entry) entries;
};

TAILQ_HEAD(, entry) head;
TAILQ_INIT(&head);

struct entry* e = malloc(sizeof(*e));
e->value = 42;
TAILQ_INSERT_TAIL(&head, e, entries);
```

---

## Memory Management

### tlsf - Two-Level Segregated Fit Allocator
- **URL:** https://github.com/mattconte/tlsf
- **License:** BSD
- **Features:** O(1) malloc/free, real-time safe, low fragmentation

```c
#include "tlsf.h"

char pool[1024 * 1024];  // 1MB pool
tlsf_t tlsf = tlsf_create_with_pool(pool, sizeof(pool));

void* ptr = tlsf_malloc(tlsf, 1024);
tlsf_free(tlsf, ptr);
```

### Memory Pool (Simple)
```c
typedef struct {
    uint8_t* buffer;
    size_t block_size;
    size_t block_count;
    uint32_t* free_list;
    size_t free_count;
} Pool;

void pool_init(Pool* p, size_t block_size, size_t count) {
    p->block_size = block_size;
    p->block_count = count;
    p->buffer = malloc(block_size * count);
    p->free_list = malloc(count * sizeof(uint32_t));
    p->free_count = count;
    for (size_t i = 0; i < count; i++)
        p->free_list[i] = i;
}

void* pool_alloc(Pool* p) {
    if (p->free_count == 0) return NULL;
    uint32_t idx = p->free_list[--p->free_count];
    return p->buffer + idx * p->block_size;
}

void pool_free(Pool* p, void* ptr) {
    uint32_t idx = ((uint8_t*)ptr - p->buffer) / p->block_size;
    p->free_list[p->free_count++] = idx;
}
```

### Arena Allocator
```c
typedef struct {
    uint8_t* buffer;
    size_t size;
    size_t offset;
} Arena;

void arena_init(Arena* a, size_t size) {
    a->buffer = malloc(size);
    a->size = size;
    a->offset = 0;
}

void* arena_alloc(Arena* a, size_t size) {
    size = (size + 7) & ~7;  // Align to 8
    if (a->offset + size > a->size) return NULL;
    void* ptr = a->buffer + a->offset;
    a->offset += size;
    return ptr;
}

void arena_reset(Arena* a) {
    a->offset = 0;
}

void arena_free(Arena* a) {
    free(a->buffer);
}
```

---

## String Utilities

### sds - Simple Dynamic Strings
- **URL:** https://github.com/antirez/sds
- **License:** BSD
- **Used in:** Redis

```c
#include "sds.h"

sds s = sdsnew("Hello");
s = sdscat(s, " World");
s = sdscatprintf(s, " %d", 42);
printf("%s (len=%zu)\n", s, sdslen(s));
sdsfree(s);
```

### stb_sprintf.h - Fast sprintf
- **URL:** https://github.com/nothings/stb
- **License:** Public Domain
- **Features:** Faster than standard sprintf, no malloc

```c
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

char buf[256];
stbsp_sprintf(buf, "Value: %d, Float: %.2f", 42, 3.14);
```

### String Builder (Simple)
```c
typedef struct {
    char* data;
    size_t len;
    size_t cap;
} StringBuilder;

void sb_init(StringBuilder* sb) {
    sb->cap = 64;
    sb->data = malloc(sb->cap);
    sb->len = 0;
    sb->data[0] = '\0';
}

void sb_append(StringBuilder* sb, const char* str) {
    size_t slen = strlen(str);
    if (sb->len + slen + 1 > sb->cap) {
        sb->cap = (sb->len + slen + 1) * 2;
        sb->data = realloc(sb->data, sb->cap);
    }
    memcpy(sb->data + sb->len, str, slen + 1);
    sb->len += slen;
}

void sb_appendf(StringBuilder* sb, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sb_append(sb, buf);
}
```

---

## Parsing & Serialization

### cJSON - JSON Parser
- **URL:** https://github.com/DaveGamble/cJSON
- **License:** MIT
- **Size:** 2 files (~3000 lines)

```c
#include "cJSON.h"

// Parse
cJSON* root = cJSON_Parse("{\"name\":\"John\",\"age\":30}");
cJSON* name = cJSON_GetObjectItem(root, "name");
printf("Name: %s\n", name->valuestring);
cJSON_Delete(root);

// Create
cJSON* obj = cJSON_CreateObject();
cJSON_AddStringToObject(obj, "name", "Jane");
cJSON_AddNumberToObject(obj, "age", 25);
char* str = cJSON_Print(obj);
printf("%s\n", str);
free(str);
cJSON_Delete(obj);
```

### minIni - INI File Parser
- **URL:** https://github.com/compuphase/minIni
- **License:** Apache 2.0
- **Size:** 2 files (~1000 lines)

```c
#include "minIni.h"

// Read
char name[64];
ini_gets("Player", "name", "Unknown", name, sizeof(name), "config.ini");
int score = ini_getl("Player", "score", 0, "config.ini");

// Write
ini_puts("Player", "name", "NewName", "config.ini");
ini_putl("Player", "score", 100, "config.ini");
```

### inih - INI Parser (Header-only)
- **URL:** https://github.com/benhoyt/inih
- **License:** BSD

```c
#include "ini.h"

int handler(void* user, const char* section, const char* name, const char* value) {
    Config* cfg = (Config*)user;
    if (strcmp(section, "player") == 0) {
        if (strcmp(name, "name") == 0)
            strcpy(cfg->name, value);
        else if (strcmp(name, "level") == 0)
            cfg->level = atoi(value);
    }
    return 1;
}

ini_parse("config.ini", handler, &config);
```

### yxml - Tiny XML Parser
- **URL:** https://dev.yorhel.nl/yxml
- **License:** MIT
- **Size:** ~1000 lines, streaming parser

```c
#include "yxml.h"

char stack[1024];
yxml_t xml;
yxml_init(&xml, stack, sizeof(stack));

for (char* p = xml_data; *p; p++) {
    yxml_ret_t r = yxml_parse(&xml, *p);
    if (r == YXML_ELEMSTART)
        printf("Element: %s\n", xml.elem);
    else if (r == YXML_ATTRVAL)
        printf("  Attr value char: %c\n", *xml.data);
}
```

---

## Compression

### miniz - zlib Replacement
- **URL:** https://github.com/richgel999/miniz
- **License:** MIT
- **Features:** DEFLATE, zlib, zip archive support

```c
#define MINIZ_IMPLEMENTATION
#include "miniz.h"

// Compress
uLong src_len = strlen(source);
uLong cmp_len = compressBound(src_len);
uint8_t* compressed = malloc(cmp_len);
compress(compressed, &cmp_len, (uint8_t*)source, src_len);

// Decompress
uLong dst_len = src_len;
uint8_t* decompressed = malloc(dst_len);
uncompress(decompressed, &dst_len, compressed, cmp_len);
```

### lz4 - Fast Compression
- **URL:** https://github.com/lz4/lz4
- **License:** BSD
- **Features:** Extremely fast, moderate compression

```c
#include "lz4.h"

int src_size = strlen(source);
int max_dst = LZ4_compressBound(src_size);
char* compressed = malloc(max_dst);
int cmp_size = LZ4_compress_default(source, compressed, src_size, max_dst);

char* decompressed = malloc(src_size);
LZ4_decompress_safe(compressed, decompressed, cmp_size, src_size);
```

### heatshrink - Embedded Compression
- **URL:** https://github.com/atomicobject/heatshrink
- **License:** ISC
- **Features:** <1KB code, static memory, for embedded

---

## Cryptography & Hashing

### TweetNaCl - Minimal Crypto
- **URL:** https://tweetnacl.cr.yp.to/
- **License:** Public Domain
- **Size:** ~100 tweets worth of code
- **Features:** Public-key crypto, secret-key crypto, hashing

### xxHash - Fast Hashing
- **URL:** https://github.com/Cyan4973/xxHash
- **License:** BSD
- **Features:** Extremely fast non-crypto hash

```c
#define XXH_INLINE_ALL
#include "xxhash.h"

XXH64_hash_t hash = XXH64(data, length, seed);
```

### MD5 / SHA1 (Simple Implementations)
```c
// Many public domain implementations available
// Search: "md5.c public domain"
void md5(const uint8_t* data, size_t len, uint8_t out[16]);
void sha1(const uint8_t* data, size_t len, uint8_t out[20]);
```

---

## Command Line Parsing

### getopt (Standard)
```c
#include <getopt.h>

int main(int argc, char** argv) {
    int opt;
    while ((opt = getopt(argc, argv, "hvf:")) != -1) {
        switch (opt) {
            case 'h': print_help(); break;
            case 'v': verbose = true; break;
            case 'f': filename = optarg; break;
        }
    }
}
```

### optparse - Single Header
- **URL:** https://github.com/skeeto/optparse
- **License:** Public Domain

```c
#include "optparse.h"

struct optparse options;
optparse_init(&options, argv);

int opt;
while ((opt = optparse(&options, "hvf:")) != -1) {
    switch (opt) {
        case 'h': print_help(); break;
        case 'f': filename = options.optarg; break;
    }
}
```

### argtable3 - Full Featured
- **URL:** https://github.com/argtable/argtable3
- **License:** BSD

---

## Logging

### log.c - Simple Logger
- **URL:** https://github.com/rxi/log.c
- **License:** MIT
- **Size:** ~200 lines

```c
#include "log.h"

log_set_level(LOG_DEBUG);
log_set_fp(logfile);

log_debug("Debug message: %d", value);
log_info("Information");
log_warn("Warning");
log_error("Error: %s", strerror(errno));
```

### Simple Logger (DIY)
```c
typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } LogLevel;

static LogLevel g_log_level = LOG_INFO;
static FILE* g_log_file = NULL;

void log_init(const char* filename, LogLevel level) {
    g_log_level = level;
    g_log_file = filename ? fopen(filename, "a") : stderr;
}

void log_write(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < g_log_level) return;

    static const char* level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);

    fprintf(g_log_file, "%02d:%02d:%02d [%s] %s:%d: ",
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            level_names[level], file, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_file, fmt, args);
    va_end(args);

    fprintf(g_log_file, "\n");
    fflush(g_log_file);
}

#define LOG_DEBUG(...) log_write(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_write(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_write(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
```

---

## Testing

### utest.h - Unit Testing
- **URL:** https://github.com/sheredom/utest.h
- **License:** Public Domain

```c
#include "utest.h"

UTEST(math, addition) {
    ASSERT_EQ(2 + 2, 4);
}

UTEST(math, multiplication) {
    ASSERT_EQ(3 * 4, 12);
}

UTEST_MAIN();
```

### munit - Full Featured Testing
- **URL:** https://github.com/nemequ/munit
- **License:** MIT

### MinUnit (Minimal)
```c
#define mu_assert(test, message) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { char* msg = test(); tests_run++; if (msg) return msg; } while (0)

int tests_run = 0;

char* test_math() {
    mu_assert(2 + 2 == 4, "Math is broken");
    return NULL;
}

char* all_tests() {
    mu_run_test(test_math);
    return NULL;
}

int main() {
    char* result = all_tests();
    printf("Tests run: %d\n", tests_run);
    printf("%s\n", result ? result : "ALL PASSED");
    return result != NULL;
}
```

---

## Regular Expressions

### tiny-regex-c
- **URL:** https://github.com/kokke/tiny-regex-c
- **License:** Public Domain
- **Size:** ~500 lines

```c
#include "re.h"

re_t pattern = re_compile("^[a-z]+@[a-z]+\\.[a-z]+$");
int match_length;
int idx = re_matchp(pattern, "test@example.com", &match_length);
if (idx >= 0) {
    printf("Match found at %d, length %d\n", idx, match_length);
}
```

---

## Threading (Cooperative)

### Protothreads
- **URL:** http://dunkels.com/adam/pt/
- **License:** BSD
- **Size:** ~100 lines of macros

```c
#include "pt.h"

static int counter = 0;
static struct pt pt1, pt2;

PT_THREAD(thread1(struct pt* pt)) {
    PT_BEGIN(pt);
    while (1) {
        counter++;
        printf("Thread 1: %d\n", counter);
        PT_YIELD(pt);
    }
    PT_END(pt);
}

PT_THREAD(thread2(struct pt* pt)) {
    PT_BEGIN(pt);
    while (1) {
        counter--;
        printf("Thread 2: %d\n", counter);
        PT_YIELD(pt);
    }
    PT_END(pt);
}

int main() {
    PT_INIT(&pt1);
    PT_INIT(&pt2);
    while (1) {
        thread1(&pt1);
        thread2(&pt2);
    }
}
```

### Coroutines (Simple)
```c
#define cr_begin() static int _cr_state = 0; switch(_cr_state) { case 0:
#define cr_yield() do { _cr_state = __LINE__; return; case __LINE__:; } while(0)
#define cr_end() } _cr_state = 0;

void my_coroutine() {
    cr_begin();

    printf("Step 1\n");
    cr_yield();

    printf("Step 2\n");
    cr_yield();

    printf("Step 3\n");

    cr_end();
}
```

---

## File System Utilities

### tinydir - Directory Iteration
- **URL:** https://github.com/cxong/tinydir
- **License:** BSD
- **Size:** Single header

```c
#include "tinydir.h"

tinydir_dir dir;
tinydir_open(&dir, "/path/to/dir");

while (dir.has_next) {
    tinydir_file file;
    tinydir_readfile(&dir, &file);

    printf("%s", file.name);
    if (file.is_dir) printf("/");
    printf("\n");

    tinydir_next(&dir);
}

tinydir_close(&dir);
```

### cwalk - Path Manipulation
- **URL:** https://github.com/likle/cwalk
- **License:** MIT

```c
#include "cwalk.h"

char buffer[256];
cwk_path_join("/home", "user/file.txt", buffer, sizeof(buffer));
// Result: "/home/user/file.txt"

const char* basename;
size_t len;
cwk_path_get_basename("/path/to/file.txt", &basename, &len);
// basename points to "file.txt"
```

---

## Recommended System Library Bundle

```
/usr/include/sys/
├── stb_ds.h        # Dynamic arrays, hash maps
├── stb_sprintf.h   # Fast printf
├── cJSON.h         # JSON parsing
├── minIni.h        # INI files
├── miniz.h         # Compression
├── log.h           # Logging
├── utest.h         # Unit testing
├── tinydir.h       # Directory listing
└── cwalk.h         # Path manipulation
```

## See Also

- [game_resources.md](game_resources.md) - Game development libraries
- [data_formats.md](data_formats.md) - File format parsers
- [text_processing.md](text_processing.md) - Text and string utilities
