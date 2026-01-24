# Scripting Languages for VOS

Embeddable interpreters and scripting languages written in C.

---

## Recommended: Easy to Integrate

### Lua
- **URL:** https://www.lua.org/
- **License:** MIT
- **Size:** ~25KB compiled
- **Features:** Complete language, widely used, excellent C API

```c
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

lua_State* L = luaL_newstate();
luaL_openlibs(L);  // Standard libraries

// Run Lua code
luaL_dostring(L, "print('Hello from Lua!')");

// Call Lua function from C
lua_getglobal(L, "my_function");
lua_pushnumber(L, 42);
lua_call(L, 1, 1);  // 1 arg, 1 result
int result = lua_tointeger(L, -1);
lua_pop(L, 1);

// Register C function for Lua
int my_c_func(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    lua_pushinteger(L, x * 2);
    return 1;  // Number of return values
}
lua_register(L, "double", my_c_func);

lua_close(L);
```

### MiniLua / Lua 4.0
- For even smaller footprint, use older Lua versions
- Lua 4.0 is ~15KB and simpler

### TinyScheme
- **URL:** http://tinyscheme.sourceforge.net/
- **License:** BSD
- **Size:** ~30KB
- **Features:** R5RS Scheme subset

```c
#include "scheme.h"

scheme* sc = scheme_init_new();
scheme_set_input_port_file(sc, stdin);
scheme_set_output_port_file(sc, stdout);

// Load and run
scheme_load_string(sc, "(display \"Hello Scheme!\")");

// Register C function
pointer my_func(scheme* sc, pointer args) {
    // Process args
    return sc->NIL;
}
scheme_define(sc, sc->global_env, mk_symbol(sc, "my-func"),
              mk_foreign_func(sc, my_func));

scheme_deinit(sc);
```

### wren
- **URL:** https://wren.io/
- **License:** MIT
- **Size:** ~40KB
- **Features:** Class-based, fast, simple C API

```c
#include "wren.h"

WrenConfiguration config;
wrenInitConfiguration(&config);
config.writeFn = my_write_fn;  // Print output

WrenVM* vm = wrenNewVM(&config);

wrenInterpret(vm, "main",
    "class Game {\n"
    "  static update(dt) {\n"
    "    System.print(\"Update!\")\n"
    "  }\n"
    "}\n"
);

// Call Wren method from C
WrenHandle* gameClass = wrenGetVariable(vm, "main", "Game");
WrenHandle* updateMethod = wrenMakeCallHandle(vm, "update(_)");
wrenSetSlotHandle(vm, 0, gameClass);
wrenSetSlotDouble(vm, 1, 0.016);
wrenCall(vm, updateMethod);

wrenFreeVM(vm);
```

---

## Ultra-Minimal Interpreters

### femtolisp
- **URL:** https://github.com/JeffBezanson/femtolisp
- **License:** BSD
- **Size:** ~3000 lines
- **Features:** Scheme-like, used in Julia's bootstrap

### s7 Scheme
- **URL:** https://ccrma.stanford.edu/software/snd/snd/s7.html
- **License:** BSD
- **Size:** Single file (~30K lines but very complete)
- **Features:** Embeddable, FFI, mature

### MiniLisp
- **URL:** https://github.com/rui314/minilisp
- **License:** Public Domain
- **Size:** ~1000 lines
- **Features:** Minimal Lisp, great for learning

```c
// MiniLisp is very simple
// Build it and run:
// > (define double (lambda (x) (+ x x)))
// > (double 21)
// 42
```

### tinylisp
- **URL:** https://github.com/Robert-van-Engelen/tinylisp
- **License:** BSD
- **Size:** <1000 lines
- **Features:** Minimal but complete

---

## Forth Interpreters

### zForth
- **URL:** https://github.com/zevv/zForth
- **License:** MIT
- **Size:** ~2KB compiled
- **Features:** Minimal, embeddable

```c
#include "zforth.h"

zf_init(0);  // Initialize
zf_bootstrap();  // Load bootstrap code

// Execute Forth
zf_eval(": square dup * ;");
zf_eval("5 square .");  // Prints 25
```

### pForth
- **URL:** http://www.softsynth.com/pforth/
- **License:** Public Domain
- **Features:** Portable, ANS Forth compliant

### uForth (Micro Forth)
- Various implementations under 500 lines
- Great for embedded/OS development

---

## BASIC Interpreters

### MyBasic
- **URL:** https://github.com/paladin-t/my_basic
- **License:** MIT
- **Size:** Single file (~10K lines)
- **Features:** Structured BASIC, embeddable

```c
#include "my_basic.h"

struct mb_interpreter_t* bas = NULL;
mb_init();
mb_open(&bas);

// Register C function
int my_print(struct mb_interpreter_t* s, void** l) {
    char* str = NULL;
    mb_pop_string(s, l, &str);
    printf("%s\n", str);
    return MB_FUNC_OK;
}
mb_register_func(bas, "MYPRINT", my_print);

// Run BASIC
mb_load_string(bas,
    "FOR i = 1 TO 10\n"
    "  MYPRINT \"Hello \" + STR(i)\n"
    "NEXT i\n",
    true
);
mb_run(bas, true);

mb_close(&bas);
mb_dispose();
```

### bwBASIC
- **URL:** https://sourceforge.net/projects/bwbasic/
- **License:** GPL
- **Features:** More complete BASIC

---

## Expression Evaluators

### TinyExpr
- **URL:** https://github.com/codeplea/tinyexpr
- **License:** zlib
- **Size:** ~500 lines
- **Features:** Math expression parser

```c
#include "tinyexpr.h"

double result = te_interp("sqrt(5^2+12^2)", NULL);  // 13.0

// With variables
double x = 10;
te_variable vars[] = {{"x", &x}};
te_expr* expr = te_compile("x^2 + 2*x + 1", vars, 1, NULL);
double y = te_eval(expr);  // 121.0
x = 5;
y = te_eval(expr);  // 36.0
te_free(expr);

// With functions
double my_sum(double a, double b) { return a + b; }
te_variable vars2[] = {{"sum", my_sum, TE_FUNCTION2}};
double r = te_interp("sum(3, 4)", vars2);  // 7.0
```

### expr
- **URL:** https://github.com/zserge/expr
- **License:** MIT
- **Size:** Single header
- **Features:** Expression evaluation with variables

### Duktape
- **URL:** https://duktape.org/
- **License:** MIT
- **Size:** ~200KB
- **Features:** ES5.1 JavaScript, compact, embeddable
- **Note:** Larger than others but full JavaScript

---

## Language Comparison

| Language | Size | Complexity | Best For |
|----------|------|------------|----------|
| **Lua** | 25KB | Low | General scripting |
| **TinyScheme** | 30KB | Low | Lisp enthusiasts |
| **wren** | 40KB | Low | OOP scripting |
| **MiniLisp** | 2KB | Very Low | Learning, minimal |
| **zForth** | 2KB | Very Low | Stack-based tasks |
| **TinyExpr** | 1KB | Trivial | Math only |
| **MyBasic** | 15KB | Low | Beginners |
| **Duktape** | 200KB | Medium | JavaScript |

---

## Integration Pattern for VOS

```c
// Generic script engine interface
typedef struct {
    void* state;
    int (*init)(void** state);
    void (*close)(void* state);
    int (*eval)(void* state, const char* code);
    int (*call)(void* state, const char* func, int nargs, ...);

    // Register C function
    void (*register_func)(void* state, const char* name,
                         void* func, int nargs);
} ScriptEngine;

// Lua implementation
int lua_engine_init(void** state) {
    *state = luaL_newstate();
    luaL_openlibs(*state);
    return 0;
}

void lua_engine_close(void* state) {
    lua_close(state);
}

int lua_engine_eval(void* state, const char* code) {
    return luaL_dostring(state, code);
}

ScriptEngine lua_engine = {
    .init = lua_engine_init,
    .close = lua_engine_close,
    .eval = lua_engine_eval,
    // ...
};

// Usage
ScriptEngine* engine = &lua_engine;
void* state;
engine->init(&state);
engine->eval(state, "print('Hello!')");
engine->close(state);
```

---

## Game Scripting Example

```lua
-- game_script.lua

function on_init()
    player = {
        x = 100,
        y = 100,
        speed = 200
    }
end

function on_update(dt)
    if key_pressed("left") then
        player.x = player.x - player.speed * dt
    end
    if key_pressed("right") then
        player.x = player.x + player.speed * dt
    end
end

function on_draw()
    draw_rect(player.x, player.y, 32, 32, 0xFF0000)
end
```

```c
// C integration
static int l_key_pressed(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    lua_pushboolean(L, vos_key_pressed(key));
    return 1;
}

static int l_draw_rect(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    uint32_t color = luaL_checkinteger(L, 5);
    olive_rect(canvas, x, y, w, h, color);
    return 0;
}

void game_init() {
    L = luaL_newstate();
    luaL_openlibs(L);

    lua_register(L, "key_pressed", l_key_pressed);
    lua_register(L, "draw_rect", l_draw_rect);

    luaL_dofile(L, "game_script.lua");

    lua_getglobal(L, "on_init");
    lua_call(L, 0, 0);
}

void game_update(float dt) {
    lua_getglobal(L, "on_update");
    lua_pushnumber(L, dt);
    lua_call(L, 1, 0);
}

void game_draw() {
    lua_getglobal(L, "on_draw");
    lua_call(L, 0, 0);
}
```

---

## Recommended for VOS

| Priority | Language | Why |
|----------|----------|-----|
| 1 | **Lua** | Best ecosystem, widely used |
| 2 | **TinyExpr** | Config files, calculators |
| 3 | **zForth** | OS/system scripting |
| 4 | **MyBasic** | User-friendly |

---

## See Also

- [game_resources.md](game_resources.md) - Game development
- [system_libraries.md](system_libraries.md) - System utilities
