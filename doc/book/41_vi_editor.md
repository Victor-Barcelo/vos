# Chapter 41: The Vi Text Editor

VOS includes **nextvi**, a powerful vi/ex clone that provides a complete modal text editing experience. This chapter covers everything from basic usage to advanced features like macros, registers, and syntax highlighting.

## Overview

Nextvi is a modern rewrite of the classic vi editor, originally developed by Bill Joy in 1976. It supports:

- **Modal editing** - Normal, Insert, and Ex modes
- **Syntax highlighting** - For C, Go, Python, shell scripts, and more
- **UTF-8 support** - Including bidirectional text
- **Macros** - Powerful automation capabilities
- **Multiple buffers** - Edit multiple files simultaneously
- **Registers** - Named clipboards for yanking and pasting
- **Undo/Redo** - Full history support
- **Build integration** - Compile and navigate errors with TCC

## Starting Vi

```bash
# Open vi with no file
vi

# Open a specific file
vi /tmp/hello.c

# Open multiple files
vi file1.c file2.c

# Open in ex mode (line-oriented)
vi -e file.txt
```

### Command Line Options

| Option | Description |
|--------|-------------|
| `-e` | Start in ex mode |
| `-s` | Silent mode (scripting) |
| `-m` | Disable file modification |
| `-v` | Start in visual mode (default) |

## The Three Modes

Vi operates in three primary modes:

### Normal Mode (Command Mode)

The default mode when vi starts. Keys execute commands rather than inserting text.

- Press `Esc` to return to Normal mode from any other mode
- Navigation, deletion, yanking, and other operations happen here

### Insert Mode

Text you type is inserted into the buffer.

- Enter with: `i`, `I`, `a`, `A`, `o`, `O`, `c`, `C`, `s`, `S`
- Exit with: `Esc` or `Ctrl+C`

### Ex Mode (Command-Line Mode)

Execute ex commands for file operations, search/replace, and settings.

- Enter with: `:` from Normal mode
- Exit with: `Enter` (execute) or `Esc` (cancel)

---

## Normal Mode Commands

### Cursor Movement

#### Basic Movement

| Key | Description |
|-----|-------------|
| `h` | Move left |
| `j` | Move down |
| `k` | Move up |
| `l` | Move right |
| `[#]h/j/k/l` | Move # times in direction |

#### Line Navigation

| Key | Description |
|-----|-------------|
| `0` | Beginning of line |
| `^` | First non-blank character |
| `$` | End of line |
| `[#]|` | Go to column # |

#### Word Movement

| Key | Description |
|-----|-------------|
| `w` | Next word start |
| `W` | Next WORD start (skip punctuation) |
| `b` | Previous word start |
| `B` | Previous WORD start |
| `e` | End of word |
| `E` | End of WORD |

#### Screen Movement

| Key | Description |
|-----|-------------|
| `gg` | First line of file |
| `G` | Last line of file |
| `[#]G` | Go to line # |
| `H` | Top of screen (High) |
| `M` | Middle of screen |
| `L` | Bottom of screen (Low) |
| `[#]%` | Go to #% of file |

#### Scrolling

| Key | Description |
|-----|-------------|
| `Ctrl+F` | Page forward (down) |
| `Ctrl+B` | Page backward (up) |
| `Ctrl+D` | Half-page down |
| `Ctrl+U` | Half-page up |
| `Ctrl+E` | Scroll down one line |
| `Ctrl+Y` | Scroll up one line |
| `z.` | Center screen on cursor |
| `z<Enter>` | Cursor line at top |
| `z-` | Cursor line at bottom |

#### Character Search

| Key | Description |
|-----|-------------|
| `f{char}` | Find next {char} on line |
| `F{char}` | Find previous {char} on line |
| `t{char}` | Move to before next {char} |
| `T{char}` | Move to after previous {char} |
| `;` | Repeat last f/F/t/T |
| `,` | Repeat last f/F/t/T in reverse |

#### Paragraph and Section Movement

| Key | Description |
|-----|-------------|
| `{` | Previous paragraph/section |
| `}` | Next paragraph/section |
| `(` | Previous sentence |
| `)` | Next sentence |
| `[` | Previous section (empty line) |
| `]` | Next section (empty line) |
| `%` | Matching bracket/parenthesis |

### Text Editing

#### Entering Insert Mode

| Key | Description |
|-----|-------------|
| `i` | Insert before cursor |
| `I` | Insert at beginning of line (after indent) |
| `a` | Append after cursor |
| `A` | Append at end of line |
| `o` | Open new line below |
| `O` | Open new line above |
| `s` | Substitute character (delete and insert) |
| `S` | Substitute line |
| `c{motion}` | Change (delete and insert) |
| `C` | Change to end of line |

#### Deleting Text

| Key | Description |
|-----|-------------|
| `x` | Delete character under cursor |
| `X` | Delete character before cursor |
| `d{motion}` | Delete over motion |
| `dd` | Delete entire line |
| `D` | Delete to end of line |
| `[#]x` | Delete # characters |

#### Changing Text

| Key | Description |
|-----|-------------|
| `r{char}` | Replace character with {char} |
| `R` | Show registers (in VOS nextvi) |
| `~` | Toggle case of character |
| `g~{motion}` | Toggle case over motion |
| `gu{motion}` | Lowercase over motion |
| `gU{motion}` | Uppercase over motion |

#### Joining Lines

| Key | Description |
|-----|-------------|
| `J` | Join current and next line |
| `[#]J` | Join # lines |
| `K` | Split line (insert newline) |

### Text Objects

When used with operators (d, c, y), you can operate on text objects:

| Command | Description |
|---------|-------------|
| `di"` | Delete inside double quotes |
| `di(` | Delete inside parentheses |
| `di)` | Delete inside parentheses (same) |
| `ci"` | Change inside double quotes |
| `ci(` | Change inside parentheses |

### Copying and Pasting

#### Yanking (Copying)

| Key | Description |
|-----|-------------|
| `y{motion}` | Yank (copy) over motion |
| `yy` or `Y` | Yank entire line |
| `[#]yy` | Yank # lines |

#### Pasting

| Key | Description |
|-----|-------------|
| `p` | Paste after cursor/below line |
| `P` | Paste before cursor/above line |
| `[#]p` | Paste # times |

### Undo and Redo

| Key | Description |
|-----|-------------|
| `u` | Undo last change |
| `[#]u` | Undo # changes |
| `Ctrl+R` | Redo |
| `[#]Ctrl+R` | Redo # changes |
| `.` | Repeat last command |
| `[#].` | Repeat # times |

### Search

| Key | Description |
|-----|-------------|
| `/pattern` | Search forward for pattern |
| `?pattern` | Search backward for pattern |
| `n` | Repeat search in same direction |
| `N` | Repeat search in opposite direction |
| `*` | Search for word under cursor |
| `Ctrl+A` | Search for word under cursor |

#### Search Patterns (Regex)

```
.       Any character
^       Beginning of line
$       End of line
\<      Beginning of word
\>      End of word
[abc]   Character class
[^abc]  Negated character class
*       Zero or more
+       One or more
?       Zero or one
\|      Alternation
\(  \)  Grouping
```

### Marks

Marks let you save positions and return to them later.

| Key | Description |
|-----|-------------|
| `m{a-z}` | Set mark {a-z} at cursor |
| `'{a-z}` | Jump to line of mark |
| `` `{a-z} `` | Jump to exact position of mark |
| `''` | Jump to previous position |
| ``` `` ``` | Jump to exact previous position |

#### Special Marks

| Mark | Description |
|------|-------------|
| `'*` | Previous ex command position |
| `'[` | First line of last change |
| `']` | Last line of last change |

### Registers

Registers are named storage locations for text.

#### Using Registers

| Command | Description |
|---------|-------------|
| `"{reg}y{motion}` | Yank into register {reg} |
| `"{reg}d{motion}` | Delete into register {reg} |
| `"{reg}p` | Paste from register {reg} |
| `R` | Display all registers |

#### Special Registers

| Register | Description |
|----------|-------------|
| `"` (unnamed) | Default register |
| `0` | Last yank |
| `1-9` | Delete history |
| `/` | Last search pattern |
| `:` | Last ex command |

### Macros

Macros record and replay sequences of commands.

| Key | Description |
|-----|-------------|
| `@{reg}` | Execute macro in register {reg} |
| `@@` | Repeat last macro |
| `[#]@{reg}` | Execute macro # times |
| `&{reg}` | Execute macro non-blocking |

#### Recording Macros

In VOS, macros are stored in registers. You can populate a register via ex commands:

```
:ya a      " Yank current line into register a
```

Or use the yank command with a register:

```
"ayy       " Yank line into register a
```

Then execute:

```
@a         " Execute register a as macro
```

### Buffers

Work with multiple files simultaneously.

| Key | Description |
|-----|-------------|
| `Ctrl+^` or `Ctrl+6` | Switch to previous buffer |
| `Ctrl+N` | Next buffer |
| `Ctrl+_` or `Ctrl+7` | Show buffer list |
| `\\` | Switch to file manager buffer |

### Display Options

| Key | Description |
|-----|-------------|
| `#` | Toggle line numbers |
| `V` | Toggle hidden characters (space, tab, newline) |
| `Ctrl+V` | Cycle through line motion preview modes |
| `Ctrl+C` | Toggle line motion numbers |
| `Ctrl+G` | Show file status |
| `ga` | Show character info under cursor |

### File Operations

| Key | Description |
|-----|-------------|
| `Ctrl+K` | Write file (save) |
| `ZZ` | Save and quit |
| `ZQ` | Quit without saving |
| `Zz` | Quit and submit history |

---

## Insert Mode Commands

When in Insert mode, most keys insert text. These special keys are available:

| Key | Description |
|-----|-------------|
| `Esc` or `Ctrl+C` | Exit insert mode |
| `Ctrl+H` or `Backspace` | Delete character |
| `Ctrl+W` | Delete word |
| `Ctrl+U` | Delete to beginning of line |
| `Ctrl+T` | Increase indent |
| `Ctrl+D` | Decrease indent |
| `Ctrl+V{char}` | Insert literal character |
| `Ctrl+K{c1}{c2}` | Insert digraph |
| `Enter` | Insert newline |

### Autocomplete

| Key | Description |
|-----|-------------|
| `Ctrl+N` | Autocomplete forward |
| `Ctrl+R` | Autocomplete backward |
| `Ctrl+X` | Mark position for completion |
| `Ctrl+G` | Index current buffer |
| `Ctrl+Y` | Reset autocomplete data |
| `Ctrl+B` | Print autocomplete options |

### Paste in Insert Mode

| Key | Description |
|-----|-------------|
| `Ctrl+]` | Cycle through paste registers 0-9 |
| `Ctrl+\` | Select paste register |
| `Ctrl+P` | Paste register |

---

## Ex Commands

Ex commands are entered after pressing `:` in Normal mode.

### File Operations

| Command | Description |
|---------|-------------|
| `:e file` | Edit file |
| `:e!` | Revert to saved |
| `:w` | Write (save) file |
| `:w file` | Write to file |
| `:w!` | Force write |
| `:wq` or `:x` | Write and quit |
| `:q` | Quit |
| `:q!` | Force quit (discard changes) |
| `:r file` | Read file into buffer |
| `:r !cmd` | Read command output |

### Buffer Management

| Command | Description |
|---------|-------------|
| `:b` | List buffers |
| `:b #` | Switch to buffer # |
| `:bp path` | Set buffer path |

### Navigation

| Command | Description |
|---------|-------------|
| `:#` | Go to line # |
| `:$` | Go to last line |
| `:.` | Current line |
| `:'m` | Go to mark m |

### Editing

| Command | Description |
|---------|-------------|
| `:[range]d` | Delete lines |
| `:[range]y [reg]` | Yank lines to register |
| `:[range]p` | Print lines |
| `:[range]m dest` | Move lines to dest |
| `:[range]j` | Join lines |
| `:[range]< or >` | Shift left/right |

### Search and Replace

| Command | Description |
|---------|-------------|
| `:/pattern` | Search forward |
| `:?pattern` | Search backward |
| `:[range]s/pat/rep/` | Substitute first match |
| `:[range]s/pat/rep/g` | Substitute all matches |

#### Substitution Flags

| Flag | Description |
|------|-------------|
| `g` | Global (all occurrences on line) |

#### Substitution Examples

```vim
" Replace first 'foo' with 'bar' on current line
:s/foo/bar/

" Replace all 'foo' with 'bar' on current line
:s/foo/bar/g

" Replace all 'foo' with 'bar' in entire file
:%s/foo/bar/g

" Replace with confirmation (not in nextvi)
" Use g// to search and manually change

" Delete all lines containing 'pattern'
:g/pattern/d

" Delete all empty lines
:g/^$/d

" Execute command on matching lines
:g/pattern/command
```

### Global Commands

| Command | Description |
|---------|-------------|
| `:g/pat/cmd` | Execute cmd on lines matching pat |
| `:g!/pat/cmd` | Execute cmd on lines NOT matching pat |

### Registers

| Command | Description |
|---------|-------------|
| `:reg` | Show all registers |
| `:ya [reg]` | Yank to register |
| `:pu [reg]` | Put from register |
| `:ya! reg` | Clear register |

### Settings

| Command | Description |
|---------|-------------|
| `:ai` | Toggle auto-indent |
| `:ic` | Toggle ignore case |
| `:hl` | Toggle syntax highlighting |
| `:hll` | Toggle current line highlight |
| `:hlw` | Toggle current word highlight |
| `:hlp` | Toggle bracket pair highlight |
| `:tbs #` | Set tab width to # spaces |
| `:ft [type]` | Set/show filetype |
| `:left [#]` | Set horizontal scroll |

### VOS Build Integration

VOS nextvi includes build integration for the TCC compiler:

| Command | Description |
|---------|-------------|
| `:make` | Compile current file with TCC |
| `:make args` | Compile with arguments |
| `:run` | Compile and run current file |
| `:run args` | Compile and run with arguments |
| `:cn` | Jump to next error |
| `:cp` | Jump to previous error |
| `:cl` | List all errors |

#### Build Integration Shortcuts

These normal mode shortcuts are available:

| Key | Description |
|-----|-------------|
| `gm` | Same as `:make` |
| `gr` | Same as `:run` |
| `gn` | Same as `:cn` (next error) |
| `gp` | Same as `:cp` (previous error) |
| `gl` | Same as `:cl` (list errors) |

### File Finding

| Command | Description |
|---------|-------------|
| `:f regex` | Fuzzy find in current buffer |
| `:ef regex` | Fuzzy find and open file |
| `:fd path` | Set secondary directory |
| `:fp path` | Set directory for :fd |
| `:cd path` | Change working directory |

### Miscellaneous

| Command | Description |
|---------|-------------|
| `:u` | Undo |
| `:rd` | Redo |
| `:!cmd` | Execute shell command |
| `:[range]!cmd` | Filter range through command |
| `:cm [kmap]` | Set keymap |

---

## Configuration

### The `.virc` File

Vi reads configuration from `/disk/.virc` or `/disk/etc/virc` at startup. Each line is executed as an ex command.

Example `.virc`:

```vim
" Enable auto-indent
ai1

" Set tab width to 4
tbs4

" Enable line highlighting
hll1

" Enable bracket pair highlighting
hlp1

" Enable syntax highlighting
hl1
```

Lines starting with `"` are comments.

### EXINIT Environment Variable

Set the `EXINIT` environment variable to run commands at startup:

```bash
export EXINIT="ai1:tbs4:hl1"
```

Commands are separated by `:` (the ex separator).

### Available Options

| Option | Default | Description |
|--------|---------|-------------|
| `ai` | 1 | Auto-indent new lines |
| `ic` | 1 | Ignore case in searches |
| `hl` | 1 | Syntax highlighting |
| `hll` | 0 | Highlight current line |
| `hlw` | 0 | Highlight word under cursor |
| `hlp` | 0 | Highlight matching brackets |
| `hlr` | 0 | Reverse direction highlighting |
| `tbs` | 8 | Tab stop width |
| `td` | 1 | Text direction (1=LTR, -1=RTL) |
| `order` | 1 | Character reordering |
| `shape` | 1 | Arabic script shaping |
| `lim` | -1 | Line length render limit |
| `led` | 1 | Terminal output enabled |
| `mpt` | 0 | Prompt after printing |
| `seq` | 1 | Undo/redo sequencing |
| `grp` | 0 | Regex search group |
| `sep` | `:` | Ex command separator |

---

## Syntax Highlighting

Nextvi includes syntax highlighting for several languages:

| Filetype | Extensions |
|----------|------------|
| `c` | `.c`, `.h`, `.cpp`, `.hpp`, `.cc`, `.cs` |
| `go` | `.go` |
| `py` | `.py` |
| `sh` | `.sh`, `.bash`, `.zsh` |
| `js` | `.js` |
| `html` | `.html`, `.htm`, `.css` |
| `mk` | `Makefile`, `.mk` |
| `diff` | `.patch`, `.diff` |
| `tex` | `.tex` |
| `roff` | `.ms`, `.tr`, `.roff`, `.1`-`.9` |

### Setting Filetype

```vim
" Show current filetype
:ft

" Set filetype manually
:ft c
:ft go
:ft py
```

---

## Practical Examples

### Basic Editing Workflow

```
1. Open file:           vi myfile.c
2. Navigate to line:    :50 or 50G
3. Enter insert mode:   i
4. Type your code
5. Exit insert mode:    Esc
6. Save:                :w or Ctrl+K
7. Quit:                :q
```

### Search and Replace

```vim
" Find all occurrences of 'foo'
/foo

" Replace 'foo' with 'bar' globally
:%s/foo/bar/g

" Replace only in lines 10-20
:10,20s/foo/bar/g
```

### Working with Multiple Files

```vim
" Open another file
:e other.c

" List buffers
:b

" Switch to buffer 0
:b 0

" Switch to previous buffer
Ctrl+^
```

### Copy and Paste Operations

```vim
" Copy 5 lines
5yy

" Delete 3 words into register a
"a3dw

" Paste from register a
"ap

" Copy a function (assuming cursor at opening brace)
%y
```

### Using Macros for Repetitive Tasks

```vim
" Store a command sequence in register q:
" 1. Yank a template line into register q
"qyy

" 2. Execute it to paste
@q

" Example: Add comment prefix to lines
" On first line: I// <Esc>j
" Then: @q to repeat on next line
" Or: 10@q to do 10 lines
```

### Compiling Code in VOS

```vim
" Save and compile current file
:w
:make

" If there are errors, navigate them:
:cn        " Next error
:cp        " Previous error
:cl        " List all errors

" Or use shortcuts:
gm         " Make
gn         " Next error
gp         " Previous error

" Compile and run:
:run
" or
gr
```

---

## Keyboard Quick Reference

### Essential Commands

| Key | Action |
|-----|--------|
| `i` | Insert before cursor |
| `a` | Append after cursor |
| `Esc` | Return to normal mode |
| `dd` | Delete line |
| `yy` | Yank line |
| `p` | Paste |
| `u` | Undo |
| `Ctrl+R` | Redo |
| `/` | Search |
| `n` | Next search match |
| `:w` | Save |
| `:q` | Quit |
| `:wq` | Save and quit |

### Navigation Summary

| Key | Action |
|-----|--------|
| `h j k l` | Left, down, up, right |
| `w b e` | Word forward, back, end |
| `0 ^ $` | Line start, first char, end |
| `gg G` | File start, file end |
| `Ctrl+F/B` | Page down/up |
| `/{pat}` | Search forward |
| `?{pat}` | Search backward |
| `n N` | Next/previous match |

### Editing Summary

| Key | Action |
|-----|--------|
| `x` | Delete character |
| `dw` | Delete word |
| `dd` | Delete line |
| `d$` or `D` | Delete to end of line |
| `cw` | Change word |
| `cc` | Change line |
| `c$` or `C` | Change to end of line |
| `r{c}` | Replace with character |
| `~` | Toggle case |
| `J` | Join lines |

---

## Tips and Tricks

### Efficient Navigation

- Use `f{char}` and `;` to quickly move to characters on a line
- Use `*` to search for the word under the cursor
- Use marks (`m{a-z}`) to save positions you'll return to
- Use `Ctrl+O` and `Ctrl+I` to move through jump history

### Power User Features

- **Dot command (`.`)**: Repeat the last change - very powerful for repetitive edits
- **Visual line motion (`Ctrl+V`)**: See line numbers for quick navigation with `{n}j` or `{n}k`
- **Text objects (`ci"`, `di(`)**: Operate on quoted strings, parenthesized text, etc.

### Common Workflows

**Comment multiple lines:**
1. Go to first line: `gg` or `:{line}`
2. Insert at beginning: `I`
3. Type comment prefix: `// `
4. Exit insert: `Esc`
5. Move down and repeat: `j.` (repeat for each line)

**Indent a block:**
1. Go to first line
2. Use `>{motion}` (e.g., `>}` to indent to next paragraph)
3. Or use visual line numbers and `:{range}>`

**Replace a word throughout file:**
```vim
:%s/\<oldword\>/newword/g
```

---

## Sources and References

VOS nextvi is based on the [nextvi project](https://github.com/kyx0r/nextvi), which is a rewrite of [neatvi](https://github.com/aligrudi/neatvi).

For more information about vi/vim:
- [Vi Reference Card](https://www.atmos.albany.edu/daes/atmclasses/atm350/vi_cheat_sheet.pdf)
- [Vim Documentation](https://vimdoc.sourceforge.net/)
- [OpenBSD vi(1) man page](https://man.openbsd.org/vi)

---

*Previous: [Chapter 40: Virtual Consoles](40_vconsoles.md)*
