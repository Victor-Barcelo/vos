/**
 * Serial Connection
 *
 * Handles communication with VOS via QEMU's serial port over TCP.
 * Optimized for efficiency with event-driven I/O and marker-based command detection.
 */

import * as net from "net";
import { EventEmitter } from "events";

// Unique marker for reliable command completion detection
const COMMAND_MARKER_PREFIX = "__VOS_CMD_END_";

export class SerialConnection extends EventEmitter {
  private socket: net.Socket | null = null;
  private port: number;
  private buffer: string = "";
  private connected: boolean = false;
  private dataListeners: Array<(data: string) => void> = [];

  constructor(port: number) {
    super();
    this.port = port;
  }

  async connect(): Promise<void> {
    if (this.socket) {
      return;  // Already connected
    }

    return new Promise((resolve, reject) => {
      const socket = new net.Socket();

      socket.setTimeout(10000);

      socket.on("timeout", () => {
        socket.destroy();
        reject(new Error("Serial connection timeout"));
      });

      socket.on("error", (err) => {
        this.connected = false;
        reject(new Error(`Serial connection error: ${err.message}`));
      });

      socket.on("close", () => {
        this.connected = false;
        this.socket = null;
        this.emit("close");
      });

      socket.on("data", (data) => {
        const str = data.toString();
        this.buffer += str;

        // Notify any active listeners (event-driven approach)
        for (const listener of this.dataListeners) {
          listener(str);
        }

        this.emit("data", str);
      });

      socket.on("connect", () => {
        socket.setTimeout(0);  // Clear timeout
        this.socket = socket;
        this.connected = true;
        resolve();
      });

      socket.connect(this.port, "localhost");
    });
  }

  disconnect(): void {
    if (this.socket) {
      this.socket.destroy();
      this.socket = null;
    }
    this.connected = false;
    this.buffer = "";
    this.dataListeners = [];
  }

  isConnected(): boolean {
    return this.connected && this.socket !== null;
  }

  async write(text: string): Promise<void> {
    if (!this.socket) {
      throw new Error("Not connected");
    }

    return new Promise((resolve, reject) => {
      this.socket!.write(text, (err) => {
        if (err) {
          reject(err);
        } else {
          resolve();
        }
      });
    });
  }

  /**
   * Read with event-driven waiting (more efficient than polling)
   */
  async read(timeout: number = 1000): Promise<string> {
    return new Promise((resolve) => {
      const startTime = Date.now();
      let resolved = false;

      // Check if we already have data
      if (this.buffer.length > 0) {
        const result = this.buffer;
        this.buffer = "";
        resolve(result);
        return;
      }

      // Wait for data event
      const onData = () => {
        if (resolved) return;
        if (this.buffer.length > 0) {
          resolved = true;
          cleanup();
          const result = this.buffer;
          this.buffer = "";
          resolve(result);
        }
      };

      const cleanup = () => {
        const idx = this.dataListeners.indexOf(onData);
        if (idx !== -1) {
          this.dataListeners.splice(idx, 1);
        }
      };

      this.dataListeners.push(onData);

      // Timeout
      setTimeout(() => {
        if (!resolved) {
          resolved = true;
          cleanup();
          const result = this.buffer;
          this.buffer = "";
          resolve(result);
        }
      }, timeout);
    });
  }

  /**
   * Wait for data matching a pattern (event-driven)
   */
  async waitForPattern(pattern: RegExp, timeout: number = 10000): Promise<string> {
    return new Promise((resolve, reject) => {
      const startTime = Date.now();
      let result = this.buffer;
      let resolved = false;

      // Check if already matches
      if (pattern.test(result)) {
        this.buffer = "";
        resolve(result);
        return;
      }

      const onData = (chunk: string) => {
        if (resolved) return;
        result += chunk;

        if (pattern.test(result)) {
          resolved = true;
          cleanup();
          this.buffer = "";
          resolve(result);
        }
      };

      const cleanup = () => {
        const idx = this.dataListeners.indexOf(onData);
        if (idx !== -1) {
          this.dataListeners.splice(idx, 1);
        }
      };

      // Clear buffer since we're accumulating in result
      this.buffer = "";
      this.dataListeners.push(onData);

      // Timeout
      setTimeout(() => {
        if (!resolved) {
          resolved = true;
          cleanup();
          resolve(result);  // Return what we have
        }
      }, timeout);
    });
  }

  async readUntilPrompt(timeout: number = 5000): Promise<string> {
    // Match common shell prompts
    const promptPattern = /[$#>]\s*$/;
    return this.waitForPattern(promptPattern, timeout);
  }

  /**
   * Execute command with unique marker for reliable completion detection.
   * This is more reliable than prompt detection.
   */
  async execCommand(command: string, timeout: number = 10000): Promise<string> {
    if (!this.socket) {
      throw new Error("Not connected");
    }

    // Generate unique marker
    const marker = `${COMMAND_MARKER_PREFIX}${Date.now()}_${Math.random().toString(36).slice(2, 8)}`;
    const markerPattern = new RegExp(marker);

    // Clear any pending output
    this.buffer = "";
    await this.sleep(50);
    this.buffer = "";

    // Send command with marker echo
    await this.write(`${command}; echo "${marker}"\n`);

    // Wait for marker
    const result = await this.waitForPattern(markerPattern, timeout);

    // Clean up output
    return this.cleanCommandOutput(result, command, marker);
  }

  /**
   * Execute command using traditional prompt detection (fallback)
   */
  async execCommandPrompt(command: string, timeout: number = 10000): Promise<string> {
    if (!this.socket) {
      throw new Error("Not connected");
    }

    // Clear any pending output
    this.buffer = "";
    await this.sleep(50);
    this.buffer = "";

    // Send command
    await this.write(command + "\n");

    // Wait for response using pattern matching
    const startTime = Date.now();
    let result = "";
    let promptSeen = false;

    while (Date.now() - startTime < timeout && !promptSeen) {
      const chunk = await this.read(100);
      result += chunk;

      // Check for shell prompt indicating command completed
      const lines = result.split("\n");
      const lastLine = lines[lines.length - 1].trimEnd();

      // Common prompt patterns
      if (
        lastLine.endsWith("$") ||
        lastLine.endsWith("#") ||
        lastLine.endsWith("> ") ||
        lastLine.match(/^[a-zA-Z0-9_-]+[@:][^\s]*[$#>]\s*$/) ||
        lastLine.match(/^\$ $/) ||
        lastLine.match(/^# $/)
      ) {
        promptSeen = true;
      }
    }

    return this.cleanCommandOutput(result, command, null);
  }

  /**
   * Execute multiple commands in batch (reduces round-trips)
   */
  async execBatch(commands: string[], timeout: number = 30000): Promise<Map<string, string>> {
    if (!this.socket) {
      throw new Error("Not connected");
    }

    const results = new Map<string, string>();
    const marker = `${COMMAND_MARKER_PREFIX}BATCH_${Date.now()}`;

    // Clear buffer
    this.buffer = "";
    await this.sleep(50);
    this.buffer = "";

    // Build batch script with markers between each command
    let script = "";
    for (let i = 0; i < commands.length; i++) {
      script += `${commands[i]}; echo "${marker}_${i}_END"\n`;
    }

    await this.write(script);

    // Wait for final marker
    const finalMarker = `${marker}_${commands.length - 1}_END`;
    const result = await this.waitForPattern(new RegExp(finalMarker), timeout);

    // Parse results
    for (let i = 0; i < commands.length; i++) {
      const startMarker = i === 0 ? commands[0] : `${marker}_${i - 1}_END`;
      const endMarker = `${marker}_${i}_END`;

      const startIdx = result.indexOf(startMarker);
      const endIdx = result.indexOf(endMarker);

      if (startIdx !== -1 && endIdx !== -1) {
        let output = result.slice(startIdx + startMarker.length, endIdx);
        output = output.trim();
        // Remove the echoed command if present
        const cmdLine = commands[i];
        if (output.startsWith(cmdLine)) {
          output = output.slice(cmdLine.length).trim();
        }
        results.set(commands[i], output);
      } else {
        results.set(commands[i], "");
      }
    }

    return results;
  }

  /**
   * Read screen dump from VOS (text mode screen buffer)
   */
  async readScreenDump(timeout: number = 5000): Promise<string> {
    return this.execCommand("screendump", timeout);
  }

  private cleanCommandOutput(result: string, command: string, marker: string | null): string {
    const lines = result.split("\n");

    // Remove the echoed command (first line)
    if (lines.length > 0 && lines[0].includes(command.split(";")[0])) {
      lines.shift();
    }

    // Remove marker line if present
    if (marker) {
      const markerIdx = lines.findIndex(line => line.includes(marker));
      if (markerIdx !== -1) {
        lines.splice(markerIdx);
      }
    }

    // Remove the final prompt line
    if (lines.length > 0) {
      const lastLine = lines[lines.length - 1].trim();
      if (
        lastLine.endsWith("$") ||
        lastLine.endsWith("#") ||
        lastLine === "$" ||
        lastLine === "#" ||
        lastLine.match(/^[a-zA-Z0-9_:-]+[$#>]\s*$/)
      ) {
        lines.pop();
      }
    }

    return lines.join("\n").trim();
  }

  private sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }
}
