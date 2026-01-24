# Chapter 28: BASIC Interpreter

VOS includes a BASIC interpreter based on uBASIC by Adam Dunkels. This provides a simple, interactive programming environment.

## Overview

uBASIC is a minimalist BASIC interpreter designed for embedded systems:

- **Integer arithmetic only** (no floating point)
- **26 variables** (a-z)
- **Line numbers** required
- **Small footprint** (~2KB code)

## Running BASIC

### List Demo Programs

```bash
basic
```

Output:
```
=== BASIC demo programs ===
 1. fibonacci - Fibonacci sequence
 2. primes - Prime number finder
 3. multable - Multiplication table
 4. factorial - Factorial calculator
 5. pyramid - Number pyramid
 6. powers2 - Powers of 2
...
```

### Run a Demo

```bash
basic -d 1
```

### Run a File

```bash
basic myprogram.bas
```

## Language Features

### Variables

26 integer variables: `a` through `z`

```basic
10 let a = 42
20 let b = a + 10
30 print b
```

### Operators

| Operator | Description |
|----------|-------------|
| + | Addition |
| - | Subtraction |
| * | Multiplication |
| / | Division |
| % | Modulo |
| & | Bitwise AND |
| \| | Bitwise OR |

### Comparisons

| Operator | Description |
|----------|-------------|
| = | Equal |
| < | Less than |
| > | Greater than |

### Statements

#### PRINT

```basic
10 print "Hello, World!"
20 print a
30 print "Value is", a
40 print a,          ' Trailing comma suppresses newline
```

#### LET

```basic
10 let x = 10
20 let y = x * 2 + 5
```

#### IF/THEN/ELSE

```basic
10 if a > 10 then print "big"
20 if a = 0 then let b = 1 else let b = 0
```

#### GOTO

```basic
10 print "loop"
20 goto 10
```

#### GOSUB/RETURN

```basic
10 gosub 100
20 print "back"
30 end
100 print "subroutine"
110 return
```

#### FOR/NEXT

```basic
10 for i = 1 to 10
20 print i
30 next i
```

With step:
```basic
10 for i = 10 to 0 step -1
20 print i
30 next i
```

#### INPUT

```basic
10 print "Enter a number:"
20 input a
30 print "You entered", a
```

#### DIM (Arrays)

```basic
10 dim a(10)
20 let a(0) = 5
30 let a(1) = 10
40 print a(0) + a(1)
```

#### END

```basic
100 end
```

### Built-in Functions

#### RND

Random number:
```basic
10 let r = rnd(100)    ' Random 0-99
20 print r
```

### String Variables

String variables use `$` suffix:
```basic
10 let a$ = "Hello"
20 print a$
```

## Graphics Mode

uBASIC includes simple graphics commands:

### GRAPHICS

Switch to graphics mode:
```basic
10 graphics 320, 200    ' Width, height
```

### TEXT

Return to text mode:
```basic
100 text
```

### CLS

Clear screen:
```basic
20 cls
```

### PSET

Plot a pixel:
```basic
30 pset 160, 100, 15    ' x, y, color
```

### LINE

Draw a line:
```basic
40 line 0, 0, 319, 199, 14    ' x1, y1, x2, y2, color
```

## Example Programs

### Fibonacci Sequence

```basic
10 print "=== Fibonacci Sequence ==="
20 let a = 0
30 let b = 1
40 for i = 1 to 20
50 print a
60 let c = a + b
70 let a = b
80 let b = c
90 next i
100 end
```

### Prime Numbers

```basic
10 print "Primes from 2 to 100:"
20 for n = 2 to 100
30 let p = 1
40 for d = 2 to n - 1
50 let r = n % d
60 if r = 0 then let p = 0
70 next d
80 if p = 1 then print n
90 next n
100 end
```

### Guess the Number

```basic
10 let n = rnd(100) + 1
20 let g = 0
30 let t = 0
40 print "Guess a number 1-100:"
50 input g
60 let t = t + 1
70 if g < n then print "Too low!"
80 if g > n then print "Too high!"
90 if g = n then goto 110
100 goto 50
110 print "Correct! Tries:", t
120 end
```

### Multiplication Table

```basic
10 print "Multiplication Table"
20 for i = 1 to 9
30 for j = 1 to 9
40 let r = i * j
50 print r,
60 next j
70 print ""
80 next i
90 end
```

### Simple Animation (Graphics)

```basic
10 graphics 320, 200
20 for x = 0 to 300
30 cls
40 pset x, 100, 15
50 next x
60 text
70 end
```

## Implementation Details

### Tokenizer

The tokenizer converts BASIC source into tokens:

```c
typedef enum {
    TOKENIZER_ERROR,
    TOKENIZER_ENDOFINPUT,
    TOKENIZER_NUMBER,
    TOKENIZER_STRING,
    TOKENIZER_VARIABLE,
    TOKENIZER_LET,
    TOKENIZER_PRINT,
    TOKENIZER_IF,
    TOKENIZER_THEN,
    TOKENIZER_ELSE,
    TOKENIZER_FOR,
    TOKENIZER_TO,
    TOKENIZER_NEXT,
    TOKENIZER_STEP,
    TOKENIZER_GOTO,
    TOKENIZER_GOSUB,
    TOKENIZER_RETURN,
    TOKENIZER_END,
    TOKENIZER_INPUT,
    TOKENIZER_DIM,
    TOKENIZER_RND,
    // ... more tokens
} token_t;
```

### Execution Model

```c
void ubasic_init(const char *program);  // Initialize
void ubasic_run(void);                   // Execute one line
int ubasic_finished(void);               // Check completion

// Run a program
ubasic_init(program_text);
while (!ubasic_finished()) {
    ubasic_run();
}
```

### Variables

```c
#define MAX_VARNUM 26
static int variables[MAX_VARNUM];           // a-z integers
static char string_variables[MAX_VARNUM][64]; // a$-z$ strings
```

### Stack Limits

```c
#define MAX_GOSUB_STACK_DEPTH 10
#define MAX_FOR_STACK_DEPTH 4
```

## Limitations

- **Integer only**: No floating point
- **26 variables**: a-z only
- **Line numbers required**: No labels
- **Limited stack depth**: 10 GOSUB, 4 FOR nesting
- **No functions**: Only GOSUB/RETURN
- **No file I/O**: Memory programs only

## Creating BASIC Programs

### In VOS

1. Create a file using an editor:
```bash
ne myprogram.bas
```

2. Run it:
```bash
basic myprogram.bas
```

### From Host

1. Add `.bas` file to `initramfs/` directory
2. Rebuild VOS
3. Run from shell

## Integration with VOS

The BASIC interpreter uses standard C library functions for I/O:

- `printf()` for PRINT
- `fgets()` for INPUT
- VOS syscalls for graphics mode

This makes it portable between kernel and userland.

## Summary

The VOS BASIC interpreter provides:

1. **Simple programming** for quick scripts
2. **Educational value** for learning programming
3. **Interactive environment** with immediate feedback
4. **Graphics support** for visual programs
5. **Demo programs** to learn from

While limited compared to modern languages, BASIC offers an accessible entry point to VOS programming.

---

*Previous: [Chapter 27: Graphics Programming](27_graphics.md)*
*Next: [Chapter 29: Build System](29_build.md)*
