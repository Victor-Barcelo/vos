# VOS MCP Server

An MCP (Model Context Protocol) server that allows Claude to interact with VOS running in QEMU.

## Features

- **Start/Stop QEMU** - Launch VOS with configurable memory and resolution
- **Execute Commands** - Run shell commands and get output via serial console
- **Screenshots** - Capture the VOS display via QMP
- **Send Keystrokes** - Interact with editors and other programs
- **File Upload** - Transfer files to VOS

## Installation

```bash
cd tools/mcp-server
npm install
npm run build
```

## Configuration

Add to your Claude Code MCP settings (`~/.claude/settings.json`):

```json
{
  "mcpServers": {
    "vos": {
      "command": "node",
      "args": ["/path/to/vos/tools/mcp-server/dist/index.js"],
      "env": {
        "VOS_ISO": "/path/to/vos/vos.iso",
        "VOS_SERIAL_PORT": "4567",
        "VOS_QMP_PORT": "4568",
        "VOS_SCREENSHOT_DIR": "/tmp/vos-screenshots"
      }
    }
  }
}
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `VOS_ISO` | `./vos.iso` | Path to VOS ISO file |
| `VOS_SERIAL_PORT` | `4567` | TCP port for serial communication |
| `VOS_QMP_PORT` | `4568` | TCP port for QMP (QEMU Machine Protocol) |
| `VOS_SCREENSHOT_DIR` | `/tmp/vos-screenshots` | Directory for screenshots |

## Available Tools

### vos_start

Start QEMU with VOS.

```
Arguments:
  - iso_path: Path to VOS ISO (optional)
  - memory: RAM size, e.g., "128M" (optional)
  - resolution: Screen resolution, e.g., "1024x768" (optional)
```

### vos_stop

Stop the running QEMU instance.

### vos_exec

Execute a command in VOS shell and return output.

```
Arguments:
  - command: Command to execute (required)
  - timeout: Timeout in milliseconds (default: 10000)
```

### vos_read

Read available serial output without sending a command.

```
Arguments:
  - timeout: Wait time in milliseconds (default: 1000)
```

### vos_write

Send raw text to serial console (no automatic newline).

```
Arguments:
  - text: Text to send (required)
```

### vos_screenshot

Take a screenshot of VOS display.

```
Arguments:
  - filename: Screenshot filename (optional, auto-generated if omitted)
```

### vos_sendkeys

Send keystrokes to VOS (for interactive programs).

```
Arguments:
  - keys: Keys to send (required)
    Examples: "hello", "ret", "ctrl-c", "up down left right", "f1"
```

### vos_status

Check if VOS/QEMU is running.

### vos_upload

Upload a file to VOS (base64 encoded via shell).

```
Arguments:
  - local_path: Path to local file (required)
  - remote_path: Destination path in VOS (required)
```

## Usage Examples

### Basic Workflow

```
1. vos_start - Launch VOS
2. vos_exec "ls /bin" - List available commands
3. vos_exec "cat /etc/motd" - Read a file
4. vos_screenshot - Capture the display
5. vos_stop - Shut down
```

### Testing a Program

```
1. vos_start
2. vos_exec "echo 'int main() { return 42; }' > /tmp/test.c"
3. vos_exec "tcc -o /tmp/test /tmp/test.c"
4. vos_exec "/tmp/test; echo $?"
5. vos_stop
```

### Interactive Editor

```
1. vos_start
2. vos_exec "ne /tmp/file.txt"
3. vos_sendkeys "Hello World"
4. vos_sendkeys "ctrl-x"  # Exit
5. vos_screenshot
```

## Key Codes for vos_sendkeys

| Key | Code |
|-----|------|
| Enter | `ret` or `enter` |
| Escape | `esc` |
| Tab | `tab` |
| Space | `space` |
| Backspace | `backspace` |
| Delete | `delete` |
| Arrow keys | `up`, `down`, `left`, `right` |
| Function keys | `f1` through `f12` |
| Ctrl+key | `ctrl-c`, `ctrl-x`, etc. |
| Alt+key | `alt-x`, `alt-f`, etc. |

## Architecture

```
┌─────────────────┐     ┌──────────────────────────────────┐
│   Claude Code   │────>│        VOS MCP Server           │
└─────────────────┘     │  ┌──────────┐  ┌─────────────┐  │
                        │  │  Serial  │  │     QMP     │  │
                        │  │Connection│  │  (screenshots│  │
                        │  │ :4567    │  │   keys)     │  │
                        │  └────┬─────┘  └──────┬──────┘  │
                        └───────┼───────────────┼─────────┘
                                │               │
                        ┌───────┴───────────────┴─────────┐
                        │            QEMU                  │
                        │  ┌─────────────────────────┐    │
                        │  │          VOS            │    │
                        │  │  (serial on COM1)       │    │
                        │  └─────────────────────────┘    │
                        └─────────────────────────────────┘
```

## Troubleshooting

### "Not connected to VOS"

Make sure to call `vos_start` first before using other commands.

### Serial connection timeout

- Check that QEMU started successfully
- Verify the serial port is not in use by another process
- Try increasing the connection timeout

### Screenshot fails

- Ensure QMP connection is established
- Check that the screenshot directory exists and is writable

### Commands timeout

- VOS may be waiting for input or running a long operation
- Try `vos_read` to see pending output
- Use `ctrl-c` via `vos_sendkeys` to interrupt

## Development

```bash
# Build
npm run build

# Run directly with ts-node
npm run dev

# Clean build artifacts
npm run clean
```

## License

MIT
