# Chapter 35: MCP Server for LLM Integration

VOS includes a Model Context Protocol (MCP) server that enables Large Language Models (like Claude) to interact with VOS running in QEMU. This allows AI assistants to start VOS, execute commands, take screenshots, upload files, and develop software directly inside the operating system.

## Overview

The MCP server (`tools/mcp-server/`) provides a bridge between an LLM and VOS via:

1. **QEMU Management** - Start/stop VOS in QEMU with configurable resolution and memory
2. **Serial Console** - Execute commands and read output via serial connection with marker-based completion detection
3. **QMP Protocol** - Take screenshots and send keystrokes via QEMU Machine Protocol
4. **File Upload** - Transfer files from host to VOS using heredoc
5. **Batch Execution** - Run multiple commands efficiently in a single call
6. **Text Screen Dump** - Get screen content as text without screenshots

## Installation

```bash
cd tools/mcp-server
npm install
npm run build
```

### Claude Code Configuration

Add to your Claude Code MCP settings (`~/.claude/claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "vos": {
      "command": "node",
      "args": ["/path/to/vos/tools/mcp-server/dist/index.js"],
      "env": {
        "VOS_ISO": "/path/to/vos/vos.iso"
      }
    }
  }
}
```

## Available Tools

### vos_start

Start QEMU with VOS. Must be called before other commands.

```
Parameters:
  iso_path   - Path to VOS ISO (default: ./vos.iso)
  memory     - RAM size (default: 256M)
  resolution - Screen resolution (default: 1920x1080)
```

### vos_stop

Stop the running QEMU instance and clean up connections.

### vos_exec

Execute a command in the VOS shell and wait for output. Uses unique marker-based detection for reliable command completion.

```
Parameters:
  command - Command to execute
  timeout - Timeout in ms (default: 10000)
```

### vos_exec_batch

Execute multiple commands in a single call. More efficient than multiple `vos_exec` calls as it reduces round-trips.

```
Parameters:
  commands - Array of commands to execute
  timeout  - Total timeout in ms (default: 30000)

Returns:
  Formatted output with each command and its result
```

**Example:**
```
vos_exec_batch with commands=["pwd", "ls /bin", "uname -a"]
```

### vos_read

Read any available output from the serial console without sending a command.

### vos_write

Write raw text to the serial console (for interactive programs).

### vos_screenshot

Take a PNG screenshot of the VOS display. Returns the file path. Use this for graphical applications.

### vos_screendump

Get the current text screen buffer content as plain text. **Much faster than screenshot** for text-mode applications. Returns the visible text on screen.

```
Parameters:
  timeout - Timeout in ms (default: 5000)
```

This calls the VOS `screendump` shell command which outputs the text console buffer via serial.

### vos_sendkeys

Send keystrokes to VOS (for interactive programs like editors).

```
Special keys: ret, esc, tab, space, backspace, up, down, left, right
Modifiers: ctrl-c, alt-x, etc.
```

### vos_status

Check if VOS/QEMU is running and get connection status.

### vos_upload

Upload a file from the host to VOS.

```
Parameters:
  local_path  - Path to file on host
  remote_path - Destination path in VOS
```

Text files are uploaded using heredoc for clean transfer. Binary files use base64 encoding.

## VOS Device Files

VOS provides several `/dev` pseudo-devices for POSIX compatibility:

| Device | Description |
|--------|-------------|
| `/dev/tty` | Controlling terminal - access terminal even with redirected I/O |
| `/dev/null` | Discards all writes, returns EOF on read |
| `/dev/zero` | Discards all writes, returns infinite zeros on read |

**Example usage:**
```bash
# Discard output
command > /dev/null

# Create a file of zeros
dd if=/dev/zero of=/tmp/zeros bs=1024 count=10

# Access terminal in a pipeline
cat file | grep pattern | interactive_prompt < /dev/tty
```

## Efficiency Tips

### Use vos_screendump Instead of vos_screenshot

For text-mode applications, `vos_screendump` is significantly faster:

| Method | Latency | Use Case |
|--------|---------|----------|
| `vos_screenshot` | ~500ms | Graphics, games, visual validation |
| `vos_screendump` | ~50ms | Shell output, text editors, logs |

### Use vos_exec_batch for Multiple Commands

Instead of multiple `vos_exec` calls:

```
# Slow: 3 round-trips
vos_exec("pwd")
vos_exec("ls")
vos_exec("cat file.txt")

# Fast: 1 round-trip
vos_exec_batch(["pwd", "ls", "cat file.txt"])
```

### Marker-Based Command Detection

The MCP server uses unique markers to reliably detect command completion:

```
command; echo "__VOS_CMD_END_1234__"
```

This is more reliable than prompt detection, especially for commands with varying output.

## LLM Workflow Guide

### Starting a Session

1. **Start VOS:**
   ```
   vos_start with iso_path="/path/to/vos.iso"
   ```

2. **Login to VOS:**
   ```
   vos_exec with command="victor"
   ```

   VOS requires login. Use `victor` for the default user account.

3. **Verify connection:**
   ```
   vos_exec with command="uname -a"
   ```

### Writing and Compiling Code

VOS supports **heredoc syntax** for writing multi-line files directly in the shell:

```bash
cat > /tmp/hello.c << 'EOF'
#include <stdio.h>
int main() {
    puts("Hello from VOS!");
    return 0;
}
EOF
```

Then compile with TCC:
```bash
tcc -o /tmp/hello /tmp/hello.c -lc
/tmp/hello
```

### Using the Upload Tool

For larger files, use `vos_upload`:

```
vos_upload with local_path="/host/path/program.c" remote_path="/tmp/program.c"
```

### Viewing Screen Content

**For text mode (default):**
```
vos_screendump
```

**For graphical output:**
```
vos_screenshot
```

Then read the returned PNG file path to view the screen.

### Interactive Programs

For programs that need keyboard input (like editors or games):

1. Start the program with `vos_exec`
2. Use `vos_sendkeys` to send input
3. Use `vos_screendump` (text) or `vos_screenshot` (graphics) to see the display
4. Send `q` or `esc` to quit

Example with the 3D cube demo:
```
vos_exec with command="s3lcube"
vos_screenshot  # See the spinning cube
vos_sendkeys with keys="q"  # Quit
```

## Example: Complete Development Session

Here's how an LLM can write, compile, and run a graphics program:

```
1. vos_start()
2. vos_exec("victor")  # Login
3. vos_write("cat > /tmp/demo.c << 'EOF'\n")
4. vos_write("#include <syscall.h>\n")
5. vos_write("int main() {\n")
6. vos_write("  for(int i=0; i<100; i++)\n")
7. vos_write("    sys_gfx_pset(100+i, 100+i, 0xFF00FF00);\n")
8. vos_write("  sys_sleep(2000);\n")
9. vos_write("  return 0;\n")
10. vos_write("}\n")
11. vos_write("EOF\n")
12. vos_read()  # Get confirmation
13. vos_exec("tcc -o /tmp/demo /tmp/demo.c -lc")
14. vos_exec("/tmp/demo")
15. vos_screenshot()  # See the result
16. vos_stop()
```

## Architecture

```
┌─────────────────┐     ┌─────────────────┐
│   LLM (Claude)  │────▶│   MCP Server    │
└─────────────────┘     │   (v2.0)        │
                        └────────┬────────┘
                                 │
                    ┌────────────┴────────────┐
                    │                         │
              ┌─────▼─────┐           ┌──────▼──────┐
              │  Serial   │           │    QMP      │
              │  (TCP)    │           │   (TCP)     │
              │  :4567    │           │   :4568     │
              └─────┬─────┘           └──────┬──────┘
                    │                        │
                    │  Marker-based          │  Screenshots
                    │  command exec          │  Keystrokes
                    │                        │
                    └──────────┬─────────────┘
                               │
                    ┌──────────▼──────────┐
                    │   QEMU + VOS        │
                    │  ┌──────────────┐   │
                    │  │  VOS Shell   │   │
                    │  │  /dev/tty    │   │
                    │  │  /dev/null   │   │
                    │  │  /dev/zero   │   │
                    │  └──────────────┘   │
                    └─────────────────────┘
```

### Serial Connection (Port 4567)

- Used for command execution and text I/O
- Connects to VOS kernel's serial driver
- Commands sent with unique markers for reliable completion detection
- Event-driven data handling for low latency

### QMP Connection (Port 4568)

- QEMU Machine Protocol for VM control
- Used for screenshots (`screendump` command)
- Used for sending keystrokes (`send-key` command)

## Troubleshooting

### "Not connected to VOS"
Call `vos_start` first to start QEMU and establish connections.

### Commands timing out
- Increase the timeout parameter
- Check if VOS is waiting for input
- Use `vos_screendump` to see current screen state

### Commands not completing
The marker-based detection should handle most cases. If issues persist:
- Try `vos_read` to see pending output
- Use `vos_sendkeys` with `ctrl-c` to interrupt stuck commands

### Screenshots show wrong format
The MCP server uses `format: "png"` for QEMU screendump. Requires QEMU 5.2+.

### Heredoc not working
Make sure you're using the updated VOS with heredoc shell support. The delimiter must appear alone on a line to end input.

## Configuration

Environment variables:

| Variable | Description | Default |
|----------|-------------|---------|
| VOS_ISO | Path to VOS ISO file | ./vos.iso |
| VOS_SERIAL_PORT | Serial port number | 4567 |
| VOS_QMP_PORT | QMP port number | 4568 |
| VOS_SCREENSHOT_DIR | Screenshot directory | /tmp/vos-screenshots |

## Summary

The VOS MCP server v2.0 enables powerful AI-assisted development:

- **Efficient command execution** - Marker-based detection and batch execution
- **Fast text capture** - `vos_screendump` for instant text-mode screen content
- **Visual feedback** - Screenshots for graphical output
- **Interactive control** - Keystrokes enable use of editors and applications
- **File transfer** - Upload source files from host for compilation
- **POSIX devices** - `/dev/null`, `/dev/zero`, `/dev/tty` for standard workflows

This creates a complete development environment that AI assistants can use to build and test software for VOS efficiently.

---

*Previous: [Chapter 34: Syscall Quick Reference](34_syscall_reference.md)*
