/**
 * Serial Connection
 *
 * Handles communication with VOS via QEMU's serial port over TCP.
 */

import * as net from "net";

export class SerialConnection {
  private socket: net.Socket | null = null;
  private port: number;
  private buffer: string = "";
  private connected: boolean = false;

  constructor(port: number) {
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
      });

      socket.on("data", (data) => {
        this.buffer += data.toString();
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

  async read(timeout: number = 1000): Promise<string> {
    const startTime = Date.now();

    while (Date.now() - startTime < timeout) {
      if (this.buffer.length > 0) {
        const result = this.buffer;
        this.buffer = "";
        return result;
      }
      await this.sleep(50);
    }

    // Return whatever we have
    const result = this.buffer;
    this.buffer = "";
    return result;
  }

  async readUntilPrompt(timeout: number = 5000): Promise<string> {
    const startTime = Date.now();
    let result = "";

    while (Date.now() - startTime < timeout) {
      await this.sleep(100);

      if (this.buffer.length > 0) {
        result += this.buffer;
        this.buffer = "";

        // Check for common shell prompts
        const trimmed = result.trimEnd();
        if (
          trimmed.endsWith("$") ||
          trimmed.endsWith("#") ||
          trimmed.endsWith(">") ||
          trimmed.endsWith("% ")
        ) {
          return result;
        }
      }
    }

    return result;
  }

  async execCommand(command: string, timeout: number = 10000): Promise<string> {
    if (!this.socket) {
      throw new Error("Not connected");
    }

    // Clear any pending output
    this.buffer = "";
    await this.sleep(100);
    this.buffer = "";

    // Send command
    await this.write(command + "\n");

    // Wait for response
    const startTime = Date.now();
    let result = "";
    let promptSeen = false;

    while (Date.now() - startTime < timeout && !promptSeen) {
      await this.sleep(100);

      if (this.buffer.length > 0) {
        result += this.buffer;
        this.buffer = "";

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
    }

    // Clean up output
    // Remove the echoed command (first line)
    const lines = result.split("\n");
    if (lines.length > 0 && lines[0].includes(command)) {
      lines.shift();
    }

    // Remove the final prompt line
    if (lines.length > 0) {
      const lastLine = lines[lines.length - 1].trim();
      if (
        lastLine.endsWith("$") ||
        lastLine.endsWith("#") ||
        lastLine === "$" ||
        lastLine === "#"
      ) {
        lines.pop();
      }
    }

    return lines.join("\n").trim();
  }

  async waitForOutput(pattern: RegExp, timeout: number = 10000): Promise<string> {
    const startTime = Date.now();
    let result = "";

    while (Date.now() - startTime < timeout) {
      await this.sleep(100);

      if (this.buffer.length > 0) {
        result += this.buffer;
        this.buffer = "";

        if (pattern.test(result)) {
          return result;
        }
      }
    }

    return result;
  }

  private sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }
}
