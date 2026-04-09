import { spawn, ChildProcess } from "node:child_process";
import { resolve } from "node:path";
import { existsSync, readFileSync } from "node:fs";
import { homedir } from "node:os";

interface MoonBitProcessConfig {
  enabled: boolean;
  binaryPath?: string;
  maxRestarts: number;
  restartDelayMs: number;
  timeoutMs: number;
}

interface MoonBitProcessManagerOptions {
  config?: Partial<MoonBitProcessConfig>;
}

export class MoonBitProcessManager {
  private process: ChildProcess | null = null;
  private restartCount = 0;
  private config: MoonBitProcessConfig;
  private readyPromise: Promise<void> | null = null;

  constructor(options: MoonBitProcessManagerOptions = {}) {
    this.config = this.loadConfig(options.config ?? {});
  }

  private loadConfig(override: Partial<MoonBitProcessConfig>): MoonBitProcessConfig {
    // 1. 从 ~/.paperclip/config.json 读取
    const homeConfig = this.readHomeConfig();
    
    // 2. 从项目 .paperclip/config.json 读取
    const projectConfig = this.readProjectConfig();

    // 3. 合并: defaults < home < project < override
    return {
      enabled: override.enabled ?? projectConfig.enabled ?? homeConfig.enabled ?? true,
      binaryPath: override.binaryPath ?? projectConfig.binaryPath ?? homeConfig.binaryPath,
      maxRestarts: override.maxRestarts ?? projectConfig.maxRestarts ?? homeConfig.maxRestarts ?? 3,
      restartDelayMs: override.restartDelayMs ?? projectConfig.restartDelayMs ?? homeConfig.restartDelayMs ?? 1000,
      timeoutMs: override.timeoutMs ?? projectConfig.timeoutMs ?? homeConfig.timeoutMs ?? 10000,
    };
  }

  private readHomeConfig(): Partial<MoonBitProcessConfig> {
    try {
      const configPath = resolve(homedir(), ".paperclip", "config.json");
      if (existsSync(configPath)) {
        const raw = readFileSync(configPath, "utf-8");
        const config = JSON.parse(raw);
        return config.moonbit ?? {};
      }
    } catch {
      // 忽略读取错误
    }
    return {};
  }

  private readProjectConfig(): Partial<MoonBitProcessConfig> {
    try {
      const configPath = resolve(process.cwd(), ".paperclip", "config.json");
      if (existsSync(configPath)) {
        const raw = readFileSync(configPath, "utf-8");
        const config = JSON.parse(raw);
        return config.moonbit ?? {};
      }
    } catch {
      // 忽略读取错误
    }
    return {};
  }

  async start(): Promise<boolean> {
    if (!this.config.enabled) {
      return false;
    }

    if (this.process && this.process.exitCode === null) {
      return true; // 已经在运行
    }

    this.restartCount = 0;
    this.readyPromise = this.startProcess();
    
    try {
      await this.readyPromise;
      return true;
    } catch {
      return false;
    }
  }

  private async startProcess(): Promise<void> {
    return new Promise((resolve, reject) => {
      const binaryPath = this.getBinaryPath();
      
      // P1-4 修复: 不使用 shell: true
      this.process = spawn(binaryPath, [], {
        stdio: ["pipe", "pipe", "pipe"],
      });

      let ready = false;

      const timeout = setTimeout(() => {
        if (!ready) {
          this.process?.kill();
          reject(new Error(`MoonBit process did not become ready in ${this.config.timeoutMs}ms`));
        }
      }, this.config.timeoutMs);

      this.process.stdout?.on("data", (data: Buffer) => {
        const output = data.toString();
        if (output.includes("MOONBIT_READY") && !ready) {
          ready = true;
          clearTimeout(timeout);
          resolve();
        }
      });

      this.process.stderr?.on("data", (data: Buffer) => {
        process.stderr.write(`[moonbit] ${data}`);
      });

      this.process.on("exit", (code) => {
        if (!ready) {
          clearTimeout(timeout);
          reject(new Error(`MoonBit process exited with code ${code} before ready`));
        } else if (code !== null && code !== 0) {
          // 崩溃后自动重启
          this.handleCrash(code);
        }
        this.process = null;
      });
    });
  }

  private getBinaryPath(): string {
    if (this.config.binaryPath) {
      return this.config.binaryPath;
    }
    
    const ext = process.platform === "win32" ? ".exe" : "";
    return resolve(
      process.cwd(),
      "moonbit-core",
      "_build",
      "default",
      "bin",
      "server" + ext
    );
  }

  private handleCrash(code: number): void {
    if (this.restartCount >= this.config.maxRestarts) {
      process.stderr.write(
        `[moonbit] Max restart attempts (${this.config.maxRestarts}) reached. Giving up.\n`
      );
      return;
    }

    this.restartCount++;
    process.stderr.write(
      `[moonbit] Process crashed (code ${code}), restarting (${this.restartCount}/${this.config.maxRestarts})...\n`
    );

    setTimeout(() => {
      this.startProcess().catch((err) => {
        process.stderr.write(`[moonbit] Restart failed: ${err.message}\n`);
      });
    }, this.config.restartDelayMs);
  }

  getProcess(): ChildProcess | null {
    return this.process;
  }

  get isRunning(): boolean {
    return this.process !== null && this.process.exitCode === null;
  }

  stop(): void {
    if (this.process) {
      this.process.kill();
      this.process = null;
    }
  }

  getConfig(): MoonBitProcessConfig {
    return { ...this.config };
  }
}
