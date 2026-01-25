/**
 * QEMU Manager
 *
 * Handles starting/stopping QEMU and QMP communication for screenshots and key sending.
 */

import { spawn, ChildProcess } from "child_process";
import * as net from "net";
import * as fs from "fs";

export class QemuManager {
  private process: ChildProcess | null = null;
  private serialPort: number;
  private qmpPort: number;
  private qmpSocket: net.Socket | null = null;

  constructor(serialPort: number, qmpPort: number) {
    this.serialPort = serialPort;
    this.qmpPort = qmpPort;
  }

  async start(isoPath: string, memory: string, resolution: string): Promise<void> {
    if (this.process) {
      throw new Error("QEMU already running");
    }

    const [width, height] = resolution.split("x").map(Number);

    // Find disk image relative to ISO
    const isoDir = isoPath.replace(/\/[^/]+$/, "");
    const diskImage = `${isoDir}/vos-disk.img`;

    const args = [
      "-boot", "order=d",
      "-cdrom", isoPath,
      "-drive", `file=${diskImage},format=raw,if=ide,index=0,media=disk`,
      "-m", memory,
      "-vga", "none",
      "-device", `bochs-display,xres=${width},yres=${height}`,
      "-serial", `tcp:localhost:${this.serialPort},server,nowait`,
      "-qmp", `tcp:localhost:${this.qmpPort},server,nowait`,
      "-display", "gtk",  // Show window for visual feedback
    ];

    console.error(`Starting QEMU: qemu-system-i386 ${args.join(" ")}`);

    this.process = spawn("qemu-system-i386", args, {
      stdio: ["ignore", "pipe", "pipe"],
    });

    this.process.stdout?.on("data", (data) => {
      console.error(`QEMU stdout: ${data}`);
    });

    this.process.stderr?.on("data", (data) => {
      console.error(`QEMU stderr: ${data}`);
    });

    this.process.on("exit", (code) => {
      console.error(`QEMU exited with code ${code}`);
      this.process = null;
      this.qmpSocket?.destroy();
      this.qmpSocket = null;
    });

    // Wait for QEMU to start
    await this.sleep(1000);

    // Connect to QMP
    await this.connectQmp();
  }

  async stop(): Promise<void> {
    if (this.qmpSocket) {
      this.qmpSocket.destroy();
      this.qmpSocket = null;
    }

    if (this.process) {
      this.process.kill("SIGTERM");

      // Wait for graceful exit
      await new Promise<void>((resolve) => {
        const timeout = setTimeout(() => {
          if (this.process) {
            this.process.kill("SIGKILL");
          }
          resolve();
        }, 5000);

        this.process?.on("exit", () => {
          clearTimeout(timeout);
          resolve();
        });
      });

      this.process = null;
    }
  }

  isRunning(): boolean {
    return this.process !== null;
  }

  private async connectQmp(): Promise<void> {
    return new Promise((resolve, reject) => {
      const socket = new net.Socket();

      socket.setTimeout(5000);

      socket.on("timeout", () => {
        socket.destroy();
        reject(new Error("QMP connection timeout"));
      });

      socket.on("error", (err) => {
        reject(new Error(`QMP connection error: ${err.message}`));
      });

      let buffer = "";
      let negotiated = false;

      socket.on("data", (data) => {
        buffer += data.toString();

        // Process complete JSON messages
        const lines = buffer.split("\n");
        buffer = lines.pop() || "";

        for (const line of lines) {
          if (!line.trim()) continue;

          try {
            const msg = JSON.parse(line);

            // QMP greeting
            if (msg.QMP && !negotiated) {
              // Send capabilities negotiation
              socket.write('{"execute": "qmp_capabilities"}\n');
            }

            // Capabilities accepted
            if (msg.return !== undefined && !negotiated) {
              negotiated = true;
              this.qmpSocket = socket;
              socket.setTimeout(0);  // Clear timeout
              resolve();
            }
          } catch {
            // Ignore parse errors
          }
        }
      });

      socket.connect(this.qmpPort, "localhost");
    });
  }

  async screenshot(filepath: string): Promise<void> {
    if (!this.qmpSocket) {
      throw new Error("QMP not connected");
    }

    return new Promise((resolve, reject) => {
      const command = JSON.stringify({
        execute: "screendump",
        arguments: { filename: filepath, format: "png" },
      });

      let buffer = "";

      const onData = (data: Buffer) => {
        buffer += data.toString();
        const lines = buffer.split("\n");
        buffer = lines.pop() || "";

        for (const line of lines) {
          if (!line.trim()) continue;
          try {
            const msg = JSON.parse(line);
            if (msg.return !== undefined) {
              this.qmpSocket?.off("data", onData);
              resolve();
            } else if (msg.error) {
              this.qmpSocket?.off("data", onData);
              reject(new Error(msg.error.desc || "Screenshot failed"));
            }
          } catch {
            // Ignore parse errors
          }
        }
      };

      this.qmpSocket!.on("data", onData);
      this.qmpSocket!.write(command + "\n");

      // Timeout
      setTimeout(() => {
        this.qmpSocket?.off("data", onData);
        reject(new Error("Screenshot timeout"));
      }, 5000);
    });
  }

  async sendKeys(keys: string): Promise<void> {
    if (!this.qmpSocket) {
      throw new Error("QMP not connected");
    }

    // Parse keys string into QMP key events
    const qmpKeys = this.parseKeys(keys);

    for (const key of qmpKeys) {
      await this.sendKey(key);
      await this.sleep(50);  // Small delay between keys
    }
  }

  private async sendKey(key: string): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.qmpSocket) {
        reject(new Error("QMP not connected"));
        return;
      }

      const command = JSON.stringify({
        execute: "send-key",
        arguments: {
          keys: [{ type: "qcode", data: key }],
        },
      });

      let buffer = "";

      const onData = (data: Buffer) => {
        buffer += data.toString();
        const lines = buffer.split("\n");
        buffer = lines.pop() || "";

        for (const line of lines) {
          if (!line.trim()) continue;
          try {
            const msg = JSON.parse(line);
            if (msg.return !== undefined) {
              this.qmpSocket?.off("data", onData);
              resolve();
            } else if (msg.error) {
              this.qmpSocket?.off("data", onData);
              reject(new Error(msg.error.desc || "Send key failed"));
            }
          } catch {
            // Ignore parse errors
          }
        }
      };

      this.qmpSocket!.on("data", onData);
      this.qmpSocket!.write(command + "\n");

      // Timeout
      setTimeout(() => {
        this.qmpSocket?.off("data", onData);
        resolve();  // Don't fail on timeout for keys
      }, 1000);
    });
  }

  private parseKeys(keys: string): string[] {
    const result: string[] = [];

    // Handle special key sequences
    const specialKeys: Record<string, string> = {
      "ret": "ret",
      "enter": "ret",
      "esc": "esc",
      "tab": "tab",
      "space": "spc",
      "backspace": "backspace",
      "delete": "delete",
      "up": "up",
      "down": "down",
      "left": "left",
      "right": "right",
      "home": "home",
      "end": "end",
      "pgup": "pgup",
      "pgdn": "pgdn",
      "f1": "f1",
      "f2": "f2",
      "f3": "f3",
      "f4": "f4",
      "f5": "f5",
      "f6": "f6",
      "f7": "f7",
      "f8": "f8",
      "f9": "f9",
      "f10": "f10",
      "f11": "f11",
      "f12": "f12",
    };

    // Check for special sequences like "ctrl-c", "alt-x"
    const parts = keys.toLowerCase().split(/[\s,]+/);

    for (const part of parts) {
      if (part.startsWith("ctrl-")) {
        // Control sequences need special handling
        const char = part.slice(5);
        result.push(`ctrl-${char}`);
      } else if (part.startsWith("alt-")) {
        const char = part.slice(4);
        result.push(`alt-${char}`);
      } else if (specialKeys[part]) {
        result.push(specialKeys[part]);
      } else {
        // Regular characters
        for (const char of part) {
          result.push(this.charToQcode(char));
        }
      }
    }

    return result;
  }

  private charToQcode(char: string): string {
    // Map characters to QMP key codes
    const charMap: Record<string, string> = {
      "a": "a", "b": "b", "c": "c", "d": "d", "e": "e",
      "f": "f", "g": "g", "h": "h", "i": "i", "j": "j",
      "k": "k", "l": "l", "m": "m", "n": "n", "o": "o",
      "p": "p", "q": "q", "r": "r", "s": "s", "t": "t",
      "u": "u", "v": "v", "w": "w", "x": "x", "y": "y",
      "z": "z",
      "0": "0", "1": "1", "2": "2", "3": "3", "4": "4",
      "5": "5", "6": "6", "7": "7", "8": "8", "9": "9",
      " ": "spc",
      "-": "minus",
      "=": "equal",
      "[": "bracket_left",
      "]": "bracket_right",
      ";": "semicolon",
      "'": "apostrophe",
      ",": "comma",
      ".": "dot",
      "/": "slash",
      "\\": "backslash",
      "`": "grave_accent",
    };

    return charMap[char.toLowerCase()] || char.toLowerCase();
  }

  private sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
  }
}
