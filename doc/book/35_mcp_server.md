# Chapter 35: MCP Server for LLM Integration

VOS includes a Model Context Protocol (MCP) server that enables Large Language Models (like Claude) to interact with VOS running in QEMU. This allows AI assistants to start VOS, execute commands, take screenshots, upload files, and develop software directly inside the operating system.

## Overview

The MCP server (`tools/mcp-server/`) provides a bridge between an LLM and VOS via:

1. **QEMU Management** - Start/stop VOS in QEMU with configurable resolution and memory
2. **Serial Console** - Execute commands and read output via serial connection
3. **QMP Protocol** - Take screenshots and send keystrokes via QEMU Machine Protocol
4. **File Upload** - Transfer files from host to VOS using heredoc

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

Execute a command in the VOS shell and wait for output.

```
Parameters:
  command - Command to execute
  timeout - Timeout in ms (default: 10000)
```

### vos_read

Read any available output from the serial console without sending a command.

### vos_write

Write raw text to the serial console (for interactive programs).

### vos_screenshot

Take a PNG screenshot of the VOS display. Returns the file path.

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

### Taking Screenshots

To see the VOS display:
```
vos_screenshot
```

Then read the returned PNG file path to view the screen.

### Interactive Programs

For programs that need keyboard input (like editors or games):

1. Start the program with `vos_exec`
2. Use `vos_sendkeys` to send input
3. Use `vos_screenshot` to see the display
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
└─────────────────┘     └────────┬────────┘
                                 │
                    ┌────────────┴────────────┐
                    │                         │
              ┌─────▼─────┐           ┌──────▼──────┐
              │  Serial   │           │    QMP      │
              │  (TCP)    │           │   (TCP)     │
              └─────┬─────┘           └──────┬──────┘
                    │                        │
                    └──────────┬─────────────┘
                               │
                    ┌──────────▼──────────┐
                    │   QEMU + VOS        │
                    │  ┌──────────────┐   │
                    │  │  VOS Shell   │   │
                    │  └──────────────┘   │
                    └─────────────────────┘
```

### Serial Connection (Port 4567)

- Used for command execution and text I/O
- Connects to VOS kernel's serial driver
- Commands sent as text, output read until shell prompt

### QMP Connection (Port 4568)

- QEMU Machine Protocol for VM control
- Used for screenshots (`screendump` command)
- Used for sending keystrokes (`send-key` command)

## Troubleshooting

### "Not connected to VOS"
Call `vos_start` first to start QEMU and establish connections.

### Commands timing out
Increase the timeout parameter, or check if VOS is waiting for input.

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

The VOS MCP server enables powerful AI-assisted development:

- **Autonomous coding** - LLMs can write, compile, and test code in VOS
- **Visual feedback** - Screenshots allow LLMs to see graphical output
- **Interactive control** - Keystrokes enable use of editors and applications
- **File transfer** - Upload source files from host for compilation

This creates a complete development environment that AI assistants can use to build and test software for VOS.

---

*Previous: [Chapter 34: Syscall Quick Reference](34_syscall_reference.md)*
