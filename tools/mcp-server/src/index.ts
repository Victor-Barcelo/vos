#!/usr/bin/env node
/**
 * VOS MCP Server
 *
 * Provides tools for Claude to interact with VOS running in QEMU:
 * - Start/stop QEMU with VOS
 * - Execute commands via serial console
 * - Take screenshots via QMP
 * - Send keystrokes for interactive programs
 */

import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
  Tool,
} from "@modelcontextprotocol/sdk/types.js";

import { QemuManager } from "./qemu.js";
import { SerialConnection } from "./serial.js";
import * as path from "path";
import * as fs from "fs";

// Default paths - can be overridden via environment
const DEFAULT_VOS_ISO = process.env.VOS_ISO || path.join(process.cwd(), "vos.iso");
const DEFAULT_SERIAL_PORT = parseInt(process.env.VOS_SERIAL_PORT || "4567");
const DEFAULT_QMP_PORT = parseInt(process.env.VOS_QMP_PORT || "4568");
const SCREENSHOT_DIR = process.env.VOS_SCREENSHOT_DIR || "/tmp/vos-screenshots";

// Ensure screenshot directory exists
if (!fs.existsSync(SCREENSHOT_DIR)) {
  fs.mkdirSync(SCREENSHOT_DIR, { recursive: true });
}

// Tool definitions
const TOOLS: Tool[] = [
  {
    name: "vos_start",
    description: "Start QEMU with VOS. Must be called before other vos_* commands.",
    inputSchema: {
      type: "object",
      properties: {
        iso_path: {
          type: "string",
          description: `Path to VOS ISO file (default: ${DEFAULT_VOS_ISO})`,
        },
        memory: {
          type: "string",
          description: "RAM size (default: 256M)",
        },
        resolution: {
          type: "string",
          description: "Screen resolution WxH (default: 1920x1080)",
        },
      },
    },
  },
  {
    name: "vos_stop",
    description: "Stop the running QEMU instance",
    inputSchema: {
      type: "object",
      properties: {},
    },
  },
  {
    name: "vos_exec",
    description: "Execute a command in VOS shell and return the output. Waits for command to complete.",
    inputSchema: {
      type: "object",
      properties: {
        command: {
          type: "string",
          description: "Command to execute in VOS shell",
        },
        timeout: {
          type: "number",
          description: "Timeout in milliseconds (default: 10000)",
        },
      },
      required: ["command"],
    },
  },
  {
    name: "vos_read",
    description: "Read any available output from VOS serial console without sending a command",
    inputSchema: {
      type: "object",
      properties: {
        timeout: {
          type: "number",
          description: "How long to wait for output in ms (default: 1000)",
        },
      },
    },
  },
  {
    name: "vos_write",
    description: "Write raw text to VOS serial console without waiting for response",
    inputSchema: {
      type: "object",
      properties: {
        text: {
          type: "string",
          description: "Text to send (newline NOT automatically added)",
        },
      },
      required: ["text"],
    },
  },
  {
    name: "vos_screenshot",
    description: "Take a screenshot of VOS display. Returns the file path.",
    inputSchema: {
      type: "object",
      properties: {
        filename: {
          type: "string",
          description: "Screenshot filename (default: auto-generated timestamp)",
        },
      },
    },
  },
  {
    name: "vos_sendkeys",
    description: "Send keystrokes to VOS (for interactive programs like editors)",
    inputSchema: {
      type: "object",
      properties: {
        keys: {
          type: "string",
          description: "Keys to send (e.g., 'hello', 'ret' for enter, 'ctrl-c', 'up', 'down')",
        },
      },
      required: ["keys"],
    },
  },
  {
    name: "vos_status",
    description: "Check if VOS/QEMU is running and get connection status",
    inputSchema: {
      type: "object",
      properties: {},
    },
  },
  {
    name: "vos_upload",
    description: "Upload a file to VOS by writing it via shell commands (base64 encoded)",
    inputSchema: {
      type: "object",
      properties: {
        local_path: {
          type: "string",
          description: "Path to local file to upload",
        },
        remote_path: {
          type: "string",
          description: "Destination path in VOS",
        },
      },
      required: ["local_path", "remote_path"],
    },
  },
];

class VosMcpServer {
  private server: Server;
  private qemu: QemuManager;
  private serial: SerialConnection;

  constructor() {
    this.server = new Server(
      { name: "vos-mcp-server", version: "1.0.0" },
      { capabilities: { tools: {} } }
    );

    this.qemu = new QemuManager(DEFAULT_SERIAL_PORT, DEFAULT_QMP_PORT);
    this.serial = new SerialConnection(DEFAULT_SERIAL_PORT);

    this.setupHandlers();
  }

  private setupHandlers() {
    // List available tools
    this.server.setRequestHandler(ListToolsRequestSchema, async () => ({
      tools: TOOLS,
    }));

    // Handle tool calls
    this.server.setRequestHandler(CallToolRequestSchema, async (request) => {
      const { name, arguments: args } = request.params;

      try {
        const result = await this.handleToolCall(name, args || {});
        return {
          content: [{ type: "text", text: result }],
        };
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        return {
          content: [{ type: "text", text: `Error: ${message}` }],
          isError: true,
        };
      }
    });
  }

  private async handleToolCall(name: string, args: Record<string, unknown>): Promise<string> {
    switch (name) {
      case "vos_start":
        return this.handleStart(args);

      case "vos_stop":
        return this.handleStop();

      case "vos_exec":
        return this.handleExec(args);

      case "vos_read":
        return this.handleRead(args);

      case "vos_write":
        return this.handleWrite(args);

      case "vos_screenshot":
        return this.handleScreenshot(args);

      case "vos_sendkeys":
        return this.handleSendKeys(args);

      case "vos_status":
        return this.handleStatus();

      case "vos_upload":
        return this.handleUpload(args);

      default:
        throw new Error(`Unknown tool: ${name}`);
    }
  }

  private async handleStart(args: Record<string, unknown>): Promise<string> {
    const isoPath = (args.iso_path as string) || DEFAULT_VOS_ISO;
    const memory = (args.memory as string) || "256M";
    const resolution = (args.resolution as string) || "1920x1080";

    // Check if ISO exists
    if (!fs.existsSync(isoPath)) {
      throw new Error(`VOS ISO not found: ${isoPath}`);
    }

    // Start QEMU
    await this.qemu.start(isoPath, memory, resolution);

    // Wait a bit for boot
    await this.sleep(2000);

    // Connect serial
    await this.serial.connect();

    // Wait for shell prompt
    await this.sleep(3000);

    // Try to get initial prompt
    const output = await this.serial.readUntilPrompt(5000);

    return `VOS started successfully.\nInitial output:\n${output}`;
  }

  private async handleStop(): Promise<string> {
    this.serial.disconnect();
    await this.qemu.stop();
    return "VOS stopped";
  }

  private async handleExec(args: Record<string, unknown>): Promise<string> {
    if (!this.serial.isConnected()) {
      throw new Error("Not connected to VOS. Call vos_start first.");
    }

    const command = args.command as string;
    const timeout = (args.timeout as number) || 10000;

    return this.serial.execCommand(command, timeout);
  }

  private async handleRead(args: Record<string, unknown>): Promise<string> {
    if (!this.serial.isConnected()) {
      throw new Error("Not connected to VOS. Call vos_start first.");
    }

    const timeout = (args.timeout as number) || 1000;
    return this.serial.read(timeout);
  }

  private async handleWrite(args: Record<string, unknown>): Promise<string> {
    if (!this.serial.isConnected()) {
      throw new Error("Not connected to VOS. Call vos_start first.");
    }

    const text = args.text as string;
    await this.serial.write(text);
    return `Sent ${text.length} bytes`;
  }

  private async handleScreenshot(args: Record<string, unknown>): Promise<string> {
    if (!this.qemu.isRunning()) {
      throw new Error("QEMU not running. Call vos_start first.");
    }

    const filename = (args.filename as string) || `vos-${Date.now()}.png`;
    const filepath = path.join(SCREENSHOT_DIR, filename);

    await this.qemu.screenshot(filepath);

    return `Screenshot saved to: ${filepath}`;
  }

  private async handleSendKeys(args: Record<string, unknown>): Promise<string> {
    if (!this.qemu.isRunning()) {
      throw new Error("QEMU not running. Call vos_start first.");
    }

    const keys = args.keys as string;
    await this.qemu.sendKeys(keys);

    return `Sent keys: ${keys}`;
  }

  private async handleStatus(): Promise<string> {
    const qemuRunning = this.qemu.isRunning();
    const serialConnected = this.serial.isConnected();

    return JSON.stringify({
      qemu_running: qemuRunning,
      serial_connected: serialConnected,
      serial_port: DEFAULT_SERIAL_PORT,
      qmp_port: DEFAULT_QMP_PORT,
    }, null, 2);
  }

  private async handleUpload(args: Record<string, unknown>): Promise<string> {
    if (!this.serial.isConnected()) {
      throw new Error("Not connected to VOS. Call vos_start first.");
    }

    const localPath = args.local_path as string;
    const remotePath = args.remote_path as string;

    if (!fs.existsSync(localPath)) {
      throw new Error(`Local file not found: ${localPath}`);
    }

    const content = fs.readFileSync(localPath);
    const base64 = content.toString("base64");

    // Upload via echo and base64 decode
    // Split into chunks to avoid command line limits
    const chunkSize = 512;
    const chunks = [];
    for (let i = 0; i < base64.length; i += chunkSize) {
      chunks.push(base64.slice(i, i + chunkSize));
    }

    // Clear any existing file
    await this.serial.execCommand(`rm -f ${remotePath}`, 2000);

    // Write chunks
    for (const chunk of chunks) {
      await this.serial.execCommand(`echo -n "${chunk}" >> ${remotePath}.b64`, 2000);
    }

    // Decode base64 (if VOS has base64 command, otherwise this needs adjustment)
    // For now, just note that the file was uploaded as base64
    await this.serial.execCommand(`mv ${remotePath}.b64 ${remotePath}`, 2000);

    return `Uploaded ${content.length} bytes to ${remotePath} (base64 encoded - may need decoding)`;
  }

  private sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }

  async run() {
    const transport = new StdioServerTransport();
    await this.server.connect(transport);
    console.error("VOS MCP Server running on stdio");
  }
}

// Main entry point
const server = new VosMcpServer();
server.run().catch(console.error);
