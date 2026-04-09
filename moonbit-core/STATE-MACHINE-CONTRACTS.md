# State Machine Contracts

> Generated from TypeScript source code analysis
> Date: 2026-04-09
> Authority: `server/src/services/agents.ts`, `server/src/services/issues.ts`, `packages/shared/src/constants.ts`

---

## Agent State Machine

### Status Values
```typescript
// From constants.ts:LINE-13
AGENT_STATUSES = [
  "active",          // ⚠️ Independent status, NOT logical OR
  "paused",
  "idle",
  "running",
  "error",
  "pending_approval",
  "terminated"
]
```

**Critical Finding**: `active` is an INDEPENDENT enum value in TypeScript, NOT a logical OR of `idle | running`. It MUST be included in MoonBit enum.

### Transition Rules (from `agents.ts`)

The TypeScript implementation does NOT use an explicit transition table. Instead, transitions are enforced through business logic in service methods:

#### Method: `pause(id, reason)`
- **From**: Any status EXCEPT `terminated`
- **To**: `paused`
- **Constraint**: `terminated` agents cannot be paused

#### Method: `resume(id)`
- **From**: `paused` or `error` (implicit from code)
- **To**: `idle`
- **Constraints**:
  - `terminated` agents cannot be resumed
  - `pending_approval` agents cannot be resumed

#### Method: `terminate(id)`
- **From**: Any status
- **To**: `terminated`
- **Side effects**: Revokes API keys

#### Method: `activatePendingApproval(id)`
- **From**: `pending_approval`
- **To**: `idle`
- **Constraint**: Only works if status is exactly `pending_approval`

#### Method: `update(id, data)` - Status Changes
- **From**: `terminated`
  - **To**: Only `terminated` allowed (cannot un-terminate)
- **From**: `pending_approval`
  - **To**: Only `pending_approval` or `terminated` allowed (cannot directly activate)

#### Update Constraint (LINE 314-322):
```typescript
if (existing.status === "terminated" && data.status && data.status !== "terminated") {
  throw conflict("Terminated agents cannot be resumed");
}
if (
  existing.status === "pending_approval" &&
  data.status &&
  data.status !== "pending_approval" &&
  data.status !== "terminated"
) {
  throw conflict("Pending approval agents cannot be activated directly");
}
```

### Derived Transition Table (MoonBit Implementation)

Based on the business logic analysis:

```
Agent Status Transitions (VALID):
- active → paused: true
- active → error: true (implicit from system behavior)
- active → terminated: true
- paused → idle: true (via resume)
- paused → terminated: true
- idle → running: true (implicit from execution)
- idle → paused: true
- idle → error: true (implicit)
- idle → terminated: true
- running → idle: true (execution completed)
- running → error: true (execution failed)
- running → paused: true
- running → terminated: true
- error → idle: true (via resume, if not terminated)
- error → paused: true
- error → terminated: true
- pending_approval → idle: true (via activatePendingApproval)
- pending_approval → terminated: true
- terminated → ANY: false (TERMINAL STATE)
- ANY → terminated: true
```

### MoonBit Enum Definition

```moonbit
pub enum AgentStatus {
  Active           // ⚠️ MUST include - independent status in TS
  Paused
  Idle
  Running
  Error
  PendingApproval
  Terminated
} deriving (Eq, Debug)
```

---

## Issue State Machine

### Status Values
```typescript
// From constants.ts:LINE-91
ISSUE_STATUSES = [
  "backlog",
  "todo",
  "in_progress",
  "in_review",
  "done",
  "blocked",
  "cancelled"
]
```

### Critical Finding: NO EXPLICIT TRANSITION VALIDATION

From `issues.ts:LINE-44-49`:
```typescript
function assertTransition(from: string, to: string) {
  if (from === to) return;  // Same status = no-op
  if (!ALL_ISSUE_STATUSES.includes(to)) {
    throw conflict(`Unknown issue status: ${to}`);
  }
}
```

**This means**: TypeScript allows ANY → ANY transition as long as:
1. It's not a same-status update (no-op)
2. The target status is a valid status value

**However**, there are implicit conventions in the codebase:

### Side Effects (from `issues.ts:LINE-51-62`)
```typescript
function applyStatusSideEffects(status, patch) {
  if (status === "in_progress" && !patch.startedAt) {
    patch.startedAt = new Date();
  }
  if (status === "done") {
    patch.completedAt = new Date();
  }
  if (status === "cancelled") {
    patch.cancelledAt = new Date();
  }
  return patch;
}
```

### MoonBit Implementation Decision

**Option A**: Mirror TypeScript's permissive behavior (ANY → ANY)
**Option B**: Add stricter validation (recommended for type safety)

**Decision**: Implement permissive transitions to match TypeScript behavior, but document recommended transitions:

```
Issue Status Transitions (PERMISSIVE - matches TS):
- ANY → ANY: true (except same-status = no-op)
- done → ANY: true (TS allows, but semantically questionable)
- cancelled → ANY: true (TS allows, but semantically questionable)

Recommended Transitions (semantic, NOT enforced):
- backlog → todo, cancelled
- todo → in_progress, blocked, cancelled
- in_progress → in_review, blocked, todo, cancelled
- in_review → done, in_progress, todo, blocked
- blocked → todo, cancelled
- done → (terminal in practice)
- cancelled → (terminal in practice)
```

### MoonBit Enum Definition

```moonbit
pub enum IssueStatus {
  Backlog
  Todo
  InProgress
  InReview
  Done
  Blocked
  Cancelled
} deriving (Eq, Debug)
```

---

## Summary of Key Findings

1. **AgentStatus MUST include `Active`** - It's an independent enum value in TS, not a logical OR
2. **Agent transitions are enforced via business logic**, not a state machine table
3. **Issue has NO transition validation** - Any status can transition to any valid status
4. **Terminal states in practice** (not enforced):
   - Agent: `terminated` is truly terminal
   - Issue: `done` and `cancelled` are terminal in practice but not enforced

---

## References

- `server/src/services/agents.ts` - Agent service methods with transition logic
- `server/src/services/issues.ts:44-62` - Issue transition validation (permissive)
- `packages/shared/src/constants.ts:13-21` - AGENT_STATUSES definition
- `packages/shared/src/constants.ts:91-99` - ISSUE_STATUSES definition
