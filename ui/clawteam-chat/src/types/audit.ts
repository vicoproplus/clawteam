// 审计事件类型
export type AuditEventType =
  | "job-triggered"
  | "job-completed"
  | "cli-started"
  | "cli-completed"
  | "cli-failed"
  | "cli-killed"
  | "output-captured"
  | "user-paused"
  | "user-resumed"
  | "user-cancelled"
  | "process-timeout"
  | "output-error";

// 审计日志条目
export interface AuditLogEntry {
  id: string;
  timestamp: number;
  eventType: AuditEventType;
  sessionId?: string;
  targetId?: string;
  jobId?: string;
  runId?: string;
  userId?: string;
  channel?: string;
  details: Record<string, unknown>;
}
