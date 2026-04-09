import { spawn, ChildProcess } from "node:child_process";
import { resolve } from "node:path";
import { randomBytes } from "node:crypto";

interface JsonRpcRequest {
  jsonrpc: "2.0";
  method: string;
  params: Record<string, unknown>;
  id: number;
}

interface JsonRpcResponse {
  jsonrpc: "2.0";
  result?: unknown;
  error?: { code: number; message: string };
  id: number;
}

interface MoonBitClientOptions {
  binaryPath?: string;
  timeoutMs?: number;
}

interface HealthResult {
  status: string;
}

interface AgentValidateResult {
  valid: boolean;
  errors?: string[];
}

interface AgentTransitionResult {
  allowed: boolean;
  error?: string;
}

interface IssueTransitionResult {
  allowed: boolean;
  error?: string;
}

interface BudgetEnforceResult {
  allowed: boolean;
  level: "none" | "soft_warning" | "hard_block";
  message: string;
  budget_used_percent: number;
}

interface CostAggregateResult {
  total_cost_cents: number;
  event_count: number;
  by_agent: Array<[string, number]>;
}

export class MoonBitClient {
  private process: ChildProcess | null = null;
  private buffer = "";
  private pendingRequests = new Map<number, {
    resolve: (value: JsonRpcResponse) => void;
    reject: (error: Error) => void;
    timeout: ReturnType<typeof setTimeout>;
  }>();
  private nextId = 1;

  constructor(private options: MoonBitClientOptions = {}) {}

  async start(): Promise<void> {
    if (this.process) {
      throw new Error("MoonBit client already started");
    }

    const binaryPath = this.options.binaryPath ?? this.getDefaultBinaryPath();
    
    // P1-4 修复: 不使用 shell: true
    this.process = spawn(binaryPath, [], {
      stdio: ["pipe", "pipe", "pipe"],
    });

    this.process.stdout?.on("data", (data: Buffer) => {
      this.handleStdout(data.toString());
    });

    this.process.stderr?.on("data", (data: Buffer) => {
      // MoonBit 标准错误输出 (通常是编译警告等)
      process.stderr.write(`[moonbit-stderr] ${data}`);
    });

    this.process.on("exit", (code) => {
      const error = new Error(`MoonBit process exited with code ${code}`);
      for (const [, pending] of this.pendingRequests) {
        clearTimeout(pending.timeout);
        pending.reject(error);
      }
      this.pendingRequests.clear();
      this.process = null;
    });

    // 等待就绪信号
    await this.waitForReady();
  }

  private getDefaultBinaryPath(): string {
    // 根据平台推断二进制路径
    const ext = process.platform === "win32" ? ".exe" : "";
    return resolve(process.cwd(), "moonbit-core", "_build", "default", "bin", "server" + ext);
  }

  private waitForReady(): Promise<void> {
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        reject(new Error("MoonBit process did not become ready in time"));
      }, this.options.timeoutMs ?? 10000);

      const onData = (data: Buffer) => {
        const output = data.toString();
        if (output.includes("MOONBIT_READY")) {
          clearTimeout(timeout);
          this.process?.stdout?.off("data", onData);
          resolve();
        }
      };

      this.process?.stdout?.on("data", onData);
    });
  }

  private handleStdout(data: string): void {
    this.buffer += data;
    
    // 逐行处理
    const lines = this.buffer.split("\n");
    // 保留最后一个不完整的行
    this.buffer = lines.pop() ?? "";

    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed || trimmed === "MOONBIT_READY") continue;

      try {
        const response: JsonRpcResponse = JSON.parse(trimmed);
        const pending = this.pendingRequests.get(response.id);
        if (pending) {
          clearTimeout(pending.timeout);
          this.pendingRequests.delete(response.id);
          pending.resolve(response);
        }
      } catch {
        // 忽略无法解析的行 (如空行或日志)
      }
    }
  }

  private async request<T>(method: string, params: Record<string, unknown>): Promise<T> {
    if (!this.process) {
      throw new Error("MoonBit client not started");
    }

    const id = this.nextId++;
    const request: JsonRpcRequest = {
      jsonrpc: "2.0",
      method,
      params,
      id,
    };

    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pendingRequests.delete(id);
        reject(new Error(`Request '${method}' timed out after ${this.options.timeoutMs ?? 10000}ms`));
      }, this.options.timeoutMs ?? 10000);

      this.pendingRequests.set(id, {
        resolve: (resp) => {
          if (resp.error) {
            reject(new Error(`JSON-RPC error: ${resp.error.message}`));
          } else {
            resolve(resp.result as T);
          }
        },
        reject,
        timeout,
      });

      this.process?.stdin?.write(JSON.stringify(request) + "\n");
    });
  }

  async health(): Promise<HealthResult> {
    return this.request<HealthResult>("health", {});
  }

  async validateAgent(params: {
    name: string;
    role: string;
    budget_monthly_cents: number;
  }): Promise<AgentValidateResult> {
    return this.request<AgentValidateResult>("agent.validate", params);
  }

  async transitionAgent(params: {
    from: string;
    to: string;
  }): Promise<AgentTransitionResult> {
    return this.request<AgentTransitionResult>("agent.transition", params);
  }

  async transitionIssue(params: {
    from: string;
    to: string;
  }): Promise<IssueTransitionResult> {
    return this.request<IssueTransitionResult>("issue.transition", params);
  }

  async checkBudget(params: {
    company_id: string;
    agent_id: string;
    budget_monthly_cents: number;
    spent_monthly_cents: number;
    current_cost_cents: number;
  }): Promise<BudgetEnforceResult> {
    return this.request<BudgetEnforceResult>("budget.enforce", params);
  }

  async aggregateCost(params: {
    company_id: string;
    agent_ids?: string[];
    start_date?: string;
    end_date?: string;
  }): Promise<CostAggregateResult> {
    return this.request<CostAggregateResult>("cost.aggregate", params);
  }

  stop(): void {
    if (this.process) {
      this.process.kill();
      this.process = null;
    }
    for (const [, pending] of this.pendingRequests) {
      clearTimeout(pending.timeout);
      pending.reject(new Error("Client stopped"));
    }
    this.pendingRequests.clear();
  }

  get running(): boolean {
    return this.process !== null && this.process.exitCode === null;
  }
}
