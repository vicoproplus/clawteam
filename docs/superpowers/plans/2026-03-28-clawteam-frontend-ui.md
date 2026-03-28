# ClawTeam 前端 UI 组件实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 基于 Golutra UI 设计参考，修改 ClawTeam 前端组件，实现多 Agent 聊天界面，保留 Feishu/Weixin 渠道配置组件。

**架构：** 
- 采用 React 19 + Redux Toolkit + Tailwind CSS 技术栈
- 左侧 Agent 面板 + 中间聊天面板 + 右侧成员/Skill 面板的三栏布局
- Agent 角色系统（助手/员工/监工）可视化展示
- Skill 商店与配置功能

**技术栈：** React 19, Redux Toolkit, Tailwind CSS 4, Radix UI, Lucide React, Vite 7

---

## 文件结构

```
ui/clawteam-chat/src/
├── components/
│   ├── ui/                        # 基础 UI 组件（现有）
│   │   ├── avatar.tsx
│   │   ├── button.tsx
│   │   ├── input.tsx
│   │   ├── scroll-area.tsx
│   │   ├── badge.tsx              # 【新增】状态徽章
│   │   ├── card.tsx               # 【新增】卡片容器
│   │   ├── dialog.tsx             # 【新增】对话框
│   │   ├── dropdown-menu.tsx      # 【新增】下拉菜单
│   │   ├── select.tsx             # 【新增】选择器
│   │   ├── switch.tsx             # 【新增】开关
│   │   ├── tabs.tsx               # 【新增】标签页
│   │   └── textarea.tsx           # 【新增】文本域
│   ├── layout/                    # 【新增】布局组件
│   │   ├── AppLayout.tsx          # 主布局
│   │   ├── Sidebar.tsx            # 侧边栏容器
│   │   └── Header.tsx             # 顶部导航
│   ├── agent/                     # 【新增】Agent 相关组件
│   │   ├── AgentPanel.tsx         # Agent 面板
│   │   ├── AgentCard.tsx          # Agent 卡片
│   │   ├── AgentStatusBadge.tsx   # 状态徽章
│   │   └── AgentConfigModal.tsx   # Agent 配置弹窗
│   ├── skill/                     # 【新增】Skill 相关组件
│   │   ├── SkillStore.tsx         # Skill 商店
│   │   ├── SkillCard.tsx          # Skill 卡片
│   │   └── SkillDetailModal.tsx   # Skill 详情弹窗
│   ├── channel/                   # 【新增】渠道配置组件
│   │   ├── ChannelConfig.tsx      # 渠道配置面板
│   │   ├── FeishuConfig.tsx       # 飞书配置
│   │   └── WeixinConfig.tsx       # 微信配置
│   └── output/                    # 【新增】输出面板
│       └── OutputPanel.tsx        # CLI 输出面板
├── features/
│   └── chat/
│       ├── ChatInterface.tsx      # 【修改】聊天主界面
│       ├── ChatHeader.tsx         # 【新增】聊天头部
│       ├── MessageList.tsx        # 【新增】消息列表
│       ├── ChatInput.tsx          # 【新增】输入框
│       └── MessageBubble.tsx      # 【新增】消息气泡
├── store/
│   ├── index.ts
│   ├── agentSlice.ts              # 【修改】增强 Agent 状态
│   ├── chatSlice.ts
│   ├── skillSlice.ts              # 【新增】Skill 状态
│   └── channelSlice.ts            # 【新增】渠道配置状态
├── types/
│   ├── agent.ts                   # 【修改】增强类型定义
│   ├── message.ts
│   ├── session.ts
│   ├── skill.ts                   # 【新增】Skill 类型
│   └── channel.ts                 # 【新增】渠道类型
├── api/
│   ├── agent.ts
│   ├── chat.ts
│   ├── client.ts
│   ├── skill.ts                   # 【新增】Skill API
│   └── channel.ts                 # 【新增】渠道 API
├── hooks/
│   ├── useChat.ts                 # 【修改】增强聊天 Hook
│   ├── useAppDispatch.ts
│   ├── useAgents.ts               # 【新增】Agent Hook
│   ├── useSkills.ts               # 【新增】Skill Hook
│   └── useChannels.ts             # 【新增】渠道 Hook
└── lib/
    └── utils.ts
```

---

## 任务 1：基础 UI 组件扩展

**文件：**
- 创建：`ui/clawteam-chat/src/components/ui/badge.tsx`
- 创建：`ui/clawteam-chat/src/components/ui/card.tsx`
- 创建：`ui/clawteam-chat/src/components/ui/dialog.tsx`
- 创建：`ui/clawteam-chat/src/components/ui/dropdown-menu.tsx`
- 创建：`ui/clawteam-chat/src/components/ui/select.tsx`
- 创建：`ui/clawteam-chat/src/components/ui/switch.tsx`
- 创建：`ui/clawteam-chat/src/components/ui/tabs.tsx`
- 创建：`ui/clawteam-chat/src/components/ui/textarea.tsx`

- [ ] **步骤 1：安装必要的 Radix UI 依赖**

运行：
```bash
cd ui/clawteam-chat && pnpm add @radix-ui/react-dialog @radix-ui/react-dropdown-menu @radix-ui/react-select @radix-ui/react-switch @radix-ui/react-tabs @radix-ui/react-tooltip
```

预期：依赖安装成功

- [ ] **步骤 2：创建 badge.tsx 组件**

```tsx
// ui/clawteam-chat/src/components/ui/badge.tsx
import * as React from "react";
import { cva, type VariantProps } from "class-variance-authority";
import { cn } from "@/lib/utils";

const badgeVariants = cva(
  "inline-flex items-center rounded-full border px-2.5 py-0.5 text-xs font-semibold transition-colors focus:outline-none focus:ring-2 focus:ring-ring focus:ring-offset-2",
  {
    variants: {
      variant: {
        default:
          "border-transparent bg-primary text-primary-foreground hover:bg-primary/80",
        secondary:
          "border-transparent bg-secondary text-secondary-foreground hover:bg-secondary/80",
        destructive:
          "border-transparent bg-destructive text-destructive-foreground hover:bg-destructive/80",
        outline: "text-foreground",
        success:
          "border-transparent bg-green-500/20 text-green-400 border-green-500/30",
        warning:
          "border-transparent bg-yellow-500/20 text-yellow-400 border-yellow-500/30",
        working:
          "border-transparent bg-blue-500/20 text-blue-400 border-blue-500/30",
      },
    },
    defaultVariants: {
      variant: "default",
    },
  }
);

export interface BadgeProps
  extends React.HTMLAttributes<HTMLDivElement>,
    VariantProps<typeof badgeVariants> {}

function Badge({ className, variant, ...props }: BadgeProps) {
  return (
    <div className={cn(badgeVariants({ variant }), className)} {...props} />
  );
}

export { Badge, badgeVariants };
```

- [ ] **步骤 3：创建 card.tsx 组件**

```tsx
// ui/clawteam-chat/src/components/ui/card.tsx
import * as React from "react";
import { cn } from "@/lib/utils";

const Card = React.forwardRef<
  HTMLDivElement,
  React.HTMLAttributes<HTMLDivElement>
>(({ className, ...props }, ref) => (
  <div
    ref={ref}
    className={cn(
      "rounded-lg border bg-card text-card-foreground shadow-sm",
      className
    )}
    {...props}
  />
));
Card.displayName = "Card";

const CardHeader = React.forwardRef<
  HTMLDivElement,
  React.HTMLAttributes<HTMLDivElement>
>(({ className, ...props }, ref) => (
  <div
    ref={ref}
    className={cn("flex flex-col space-y-1.5 p-6", className)}
    {...props}
  />
));
CardHeader.displayName = "CardHeader";

const CardTitle = React.forwardRef<
  HTMLParagraphElement,
  React.HTMLAttributes<HTMLHeadingElement>
>(({ className, ...props }, ref) => (
  <h3
    ref={ref}
    className={cn(
      "text-2xl font-semibold leading-none tracking-tight",
      className
    )}
    {...props}
  />
));
CardTitle.displayName = "CardTitle";

const CardDescription = React.forwardRef<
  HTMLParagraphElement,
  React.HTMLAttributes<HTMLParagraphElement>
>(({ className, ...props }, ref) => (
  <p
    ref={ref}
    className={cn("text-sm text-muted-foreground", className)}
    {...props}
  />
));
CardDescription.displayName = "CardDescription";

const CardContent = React.forwardRef<
  HTMLDivElement,
  React.HTMLAttributes<HTMLDivElement>
>(({ className, ...props }, ref) => (
  <div ref={ref} className={cn("p-6 pt-0", className)} {...props} />
));
CardContent.displayName = "CardContent";

const CardFooter = React.forwardRef<
  HTMLDivElement,
  React.HTMLAttributes<HTMLDivElement>
>(({ className, ...props }, ref) => (
  <div
    ref={ref}
    className={cn("flex items-center p-6 pt-0", className)}
    {...props}
  />
));
CardFooter.displayName = "CardFooter";

export { Card, CardHeader, CardFooter, CardTitle, CardDescription, CardContent };
```

- [ ] **步骤 4：创建 dialog.tsx 组件**

```tsx
// ui/clawteam-chat/src/components/ui/dialog.tsx
import * as React from "react";
import * as DialogPrimitive from "@radix-ui/react-dialog";
import { X } from "lucide-react";
import { cn } from "@/lib/utils";

const Dialog = DialogPrimitive.Root;
const DialogTrigger = DialogPrimitive.Trigger;
const DialogPortal = DialogPrimitive.Portal;
const DialogClose = DialogPrimitive.Close;

const DialogOverlay = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Overlay>,
  React.ComponentPropsWithoutRef<typeof DialogPrimitive.Overlay>
>(({ className, ...props }, ref) => (
  <DialogPrimitive.Overlay
    ref={ref}
    className={cn(
      "fixed inset-0 z-50 bg-black/80 data-[state=open]:animate-in data-[state=closed]:animate-out data-[state=closed]:fade-out-0 data-[state=open]:fade-in-0",
      className
    )}
    {...props}
  />
));
DialogOverlay.displayName = DialogPrimitive.Overlay.displayName;

const DialogContent = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Content>,
  React.ComponentPropsWithoutRef<typeof DialogPrimitive.Content>
>(({ className, children, ...props }, ref) => (
  <DialogPortal>
    <DialogOverlay />
    <DialogPrimitive.Content
      ref={ref}
      className={cn(
        "fixed left-[50%] top-[50%] z-50 grid w-full max-w-lg translate-x-[-50%] translate-y-[-50%] gap-4 border bg-background p-6 shadow-lg duration-200 data-[state=open]:animate-in data-[state=closed]:animate-out data-[state=closed]:fade-out-0 data-[state=open]:fade-in-0 data-[state=closed]:zoom-out-95 data-[state=open]:zoom-in-95 data-[state=closed]:slide-out-to-left-1/2 data-[state=closed]:slide-out-to-top-[48%] data-[state=open]:slide-in-from-left-1/2 data-[state=open]:slide-in-from-top-[48%] sm:rounded-lg",
        className
      )}
      {...props}
    >
      {children}
      <DialogPrimitive.Close className="absolute right-4 top-4 rounded-sm opacity-70 ring-offset-background transition-opacity hover:opacity-100 focus:outline-none focus:ring-2 focus:ring-ring focus:ring-offset-2 disabled:pointer-events-none data-[state=open]:bg-accent data-[state=open]:text-muted-foreground">
        <X className="h-4 w-4" />
        <span className="sr-only">Close</span>
      </DialogPrimitive.Close>
    </DialogPrimitive.Content>
  </DialogPortal>
));
DialogContent.displayName = DialogPrimitive.Content.displayName;

const DialogHeader = ({
  className,
  ...props
}: React.HTMLAttributes<HTMLDivElement>) => (
  <div
    className={cn(
      "flex flex-col space-y-1.5 text-center sm:text-left",
      className
    )}
    {...props}
  />
);
DialogHeader.displayName = "DialogHeader";

const DialogFooter = ({
  className,
  ...props
}: React.HTMLAttributes<HTMLDivElement>) => (
  <div
    className={cn(
      "flex flex-col-reverse sm:flex-row sm:justify-end sm:space-x-2",
      className
    )}
    {...props}
  />
);
DialogFooter.displayName = "DialogFooter";

const DialogTitle = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Title>,
  React.ComponentPropsWithoutRef<typeof DialogPrimitive.Title>
>(({ className, ...props }, ref) => (
  <DialogPrimitive.Title
    ref={ref}
    className={cn(
      "text-lg font-semibold leading-none tracking-tight",
      className
    )}
    {...props}
  />
));
DialogTitle.displayName = DialogPrimitive.Title.displayName;

const DialogDescription = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Description>,
  React.ComponentPropsWithoutRef<typeof DialogPrimitive.Description>
>(({ className, ...props }, ref) => (
  <DialogPrimitive.Description
    ref={ref}
    className={cn("text-sm text-muted-foreground", className)}
    {...props}
  />
));
DialogDescription.displayName = DialogPrimitive.Description.displayName;

export {
  Dialog,
  DialogPortal,
  DialogOverlay,
  DialogClose,
  DialogTrigger,
  DialogContent,
  DialogHeader,
  DialogFooter,
  DialogTitle,
  DialogDescription,
};
```

- [ ] **步骤 5：创建其余 UI 组件（select.tsx, switch.tsx, tabs.tsx, textarea.tsx, dropdown-menu.tsx）**

参照 Radix UI 文档和 shadcn/ui 模式创建这些组件。

- [ ] **步骤 6：更新 index.css 添加动画样式**

```css
/* 添加到 ui/clawteam-chat/src/index.css */
@keyframes animate-in {
  from { opacity: 0; }
  to { opacity: 1; }
}

@keyframes fade-out {
  from { opacity: 1; }
  to { opacity: 0; }
}

.animate-in {
  animation: animate-in 0.2s ease-out;
}

.animate-out {
  animation: fade-out 0.2s ease-in;
}

/* 滚动条样式 */
::-webkit-scrollbar {
  width: 8px;
  height: 8px;
}

::-webkit-scrollbar-track {
  background: transparent;
}

::-webkit-scrollbar-thumb {
  background: rgba(255, 255, 255, 0.1);
  border-radius: 4px;
}

::-webkit-scrollbar-thumb:hover {
  background: rgba(255, 255, 255, 0.2);
}
```

- [ ] **步骤 7：Commit**

```bash
git add ui/clawteam-chat/src/components/ui/
git commit -m "feat(ui): add base UI components (badge, card, dialog, select, switch, tabs, textarea, dropdown-menu)"
```

---

## 任务 2：类型定义扩展

**文件：**
- 修改：`ui/clawteam-chat/src/types/agent.ts`
- 创建：`ui/clawteam-chat/src/types/skill.ts`
- 创建：`ui/clawteam-chat/src/types/channel.ts`

- [ ] **步骤 1：增强 agent.ts 类型定义**

```typescript
// ui/clawteam-chat/src/types/agent.ts
export type AgentRole = "assistant" | "worker" | "supervisor";
export type TerminalSessionStatus = "pending" | "online" | "working" | "offline" | "broken";
export type DispatchStrategy = "round-robin" | "least-loaded" | "capability-match" | "manual";

export interface AgentConfig {
  id: string;
  name: string;
  role: AgentRole;
  cliTool: string;
  args: string[];
  env: Record<string, string>;
  cwd?: string;
  autoStart: boolean;
  enabled: boolean;
  roleConfig?: RoleConfig;
  skills: string[];
}

export interface RoleConfig {
  // Assistant specific
  availableWorkers?: string[];
  availableSupervisors?: string[];
  dispatchStrategy?: DispatchStrategy;
  // Worker specific
  assistantId?: string;
  capabilities?: string[];
  // Supervisor specific
  reviewCriteria?: string[];
}

export interface AgentState {
  config: AgentConfig;
  status: TerminalSessionStatus;
  lastOutputAt: number;
  workingSince?: number;
}

export interface CliToolType {
  name: string;
  command: string;
  readyPatterns: string[];
  workingPatterns: string[];
}

// 角色图标映射
export const ROLE_ICONS: Record<AgentRole, string> = {
  assistant: "Bot",
  worker: "Wrench",
  supervisor: "Eye",
};

// 角色颜色映射
export const ROLE_COLORS: Record<AgentRole, { bg: string; text: string; ring: string }> = {
  assistant: { bg: "bg-purple-500/20", text: "text-purple-400", ring: "ring-purple-500/30" },
  worker: { bg: "bg-blue-500/20", text: "text-blue-400", ring: "ring-blue-500/30" },
  supervisor: { bg: "bg-green-500/20", text: "text-green-400", ring: "ring-green-500/30" },
};

// 状态颜色映射
export const STATUS_COLORS: Record<TerminalSessionStatus, string> = {
  pending: "bg-yellow-500",
  online: "bg-green-500",
  working: "bg-blue-500 animate-pulse",
  offline: "bg-gray-500",
  broken: "bg-red-500",
};
```

- [ ] **步骤 2：创建 skill.ts 类型定义**

```typescript
// ui/clawteam-chat/src/types/skill.ts
import type { AgentRole } from "./agent";

export type SkillVarType = "text" | "number" | "file" | "directory" | "select";

export interface SkillVariable {
  name: string;
  label: string;
  varType: SkillVarType;
  required: boolean;
  default?: string;
  options?: string[]; // for select type
}

export interface SkillTemplate {
  messageTemplate: string;
  variables: SkillVariable[];
  timeoutMs?: number;
}

export interface Skill {
  id: string;
  name: string;
  nameKey: string;
  description: string;
  icon: string;
  color: string;
  bg: string;
  ring: string;
  version: string;
  tags: string[];
  applicableRoles: AgentRole[];
  template: SkillTemplate;
}

export interface SkillInstance {
  skillId: string;
  variables: Record<string, string>;
  createdAt: number;
}
```

- [ ] **步骤 3：创建 channel.ts 类型定义**

```typescript
// ui/clawteam-chat/src/types/channel.ts
export type ChannelType = "feishu" | "weixin" | "web";

export interface ChannelConfig {
  type: ChannelType;
  enabled: boolean;
  appId?: string;
  appSecret?: string;
  // Feishu specific
  encryptKey?: string;
  verificationToken?: string;
  // Weixin specific
  token?: string;
  encodingAESKey?: string;
}

export interface ChannelStatus {
  type: ChannelType;
  connected: boolean;
  lastConnectedAt?: number;
  error?: string;
}
```

- [ ] **步骤 4：Commit**

```bash
git add ui/clawteam-chat/src/types/
git commit -m "feat(types): enhance agent, skill, and channel type definitions"
```

---

## 任务 3：Redux Store 扩展

**文件：**
- 修改：`ui/clawteam-chat/src/store/agentSlice.ts`
- 创建：`ui/clawteam-chat/src/store/skillSlice.ts`
- 创建：`ui/clawteam-chat/src/store/channelSlice.ts`
- 修改：`ui/clawteam-chat/src/store/index.ts`

- [ ] **步骤 1：增强 agentSlice.ts**

```typescript
// ui/clawteam-chat/src/store/agentSlice.ts
import { createSlice, type PayloadAction } from "@reduxjs/toolkit";
import type { AgentConfig, AgentState, TerminalSessionStatus, RoleConfig } from "@/types/agent";

interface AgentSliceState {
  agents: Record<string, AgentState>;
  agentList: string[];
  activeAgentId: string | null;
  cliTools: Record<string, { name: string; command: string }>;
}

const initialState: AgentSliceState = {
  agents: {},
  agentList: [],
  activeAgentId: null,
  cliTools: {},
};

const agentSlice = createSlice({
  name: "agent",
  initialState,
  reducers: {
    setAgents: (state, action: PayloadAction<AgentConfig[]>) => {
      state.agents = {};
      state.agentList = [];
      for (const config of action.payload) {
        state.agents[config.id] = { config, status: "offline", lastOutputAt: 0 };
        state.agentList.push(config.id);
      }
    },
    updateAgentStatus: (
      state,
      action: PayloadAction<{ id: string; status: TerminalSessionStatus }>
    ) => {
      const agent = state.agents[action.payload.id];
      if (agent) {
        const now = Date.now();
        if (action.payload.status === "working" && agent.status !== "working") {
          agent.workingSince = now;
        } else if (action.payload.status !== "working") {
          agent.workingSince = undefined;
        }
        agent.status = action.payload.status;
        agent.lastOutputAt = now;
      }
    },
    setActiveAgent: (state, action: PayloadAction<string | null>) => {
      state.activeAgentId = action.payload;
    },
    addAgent: (state, action: PayloadAction<AgentConfig>) => {
      const config = action.payload;
      if (!state.agents[config.id]) {
        state.agents[config.id] = { config, status: "offline", lastOutputAt: 0 };
        state.agentList.push(config.id);
      }
    },
    updateAgent: (state, action: PayloadAction<{ id: string; config: Partial<AgentConfig> }>) => {
      const agent = state.agents[action.payload.id];
      if (agent) {
        agent.config = { ...agent.config, ...action.payload.config };
      }
    },
    removeAgent: (state, action: PayloadAction<string>) => {
      delete state.agents[action.payload];
      state.agentList = state.agentList.filter((id) => id !== action.payload);
      if (state.activeAgentId === action.payload) {
        state.activeAgentId = null;
      }
    },
    setCliTools: (state, action: PayloadAction<Record<string, { name: string; command: string }>>) => {
      state.cliTools = action.payload;
    },
    updateAgentRoleConfig: (
      state,
      action: PayloadAction<{ id: string; roleConfig: RoleConfig }>
    ) => {
      const agent = state.agents[action.payload.id];
      if (agent) {
        agent.config.roleConfig = action.payload.roleConfig;
      }
    },
  },
});

export const {
  setAgents,
  updateAgentStatus,
  setActiveAgent,
  addAgent,
  updateAgent,
  removeAgent,
  setCliTools,
  updateAgentRoleConfig,
} = agentSlice.actions;

export default agentSlice.reducer;
```

- [ ] **步骤 2：创建 skillSlice.ts**

```typescript
// ui/clawteam-chat/src/store/skillSlice.ts
import { createSlice, type PayloadAction } from "@reduxjs/toolkit";
import type { Skill, SkillInstance } from "@/types/skill";

interface SkillSliceState {
  skills: Record<string, Skill>;
  skillList: string[];
  activeInstances: SkillInstance[];
  selectedSkillId: string | null;
}

const initialState: SkillSliceState = {
  skills: {},
  skillList: [],
  activeInstances: [],
  selectedSkillId: null,
};

const skillSlice = createSlice({
  name: "skill",
  initialState,
  reducers: {
    setSkills: (state, action: PayloadAction<Skill[]>) => {
      state.skills = {};
      state.skillList = [];
      for (const skill of action.payload) {
        state.skills[skill.id] = skill;
        state.skillList.push(skill.id);
      }
    },
    selectSkill: (state, action: PayloadAction<string | null>) => {
      state.selectedSkillId = action.payload;
    },
    addSkillInstance: (state, action: PayloadAction<SkillInstance>) => {
      state.activeInstances.push(action.payload);
    },
    removeSkillInstance: (state, action: PayloadAction<number>) => {
      state.activeInstances.splice(action.payload, 1);
    },
    clearSkillInstances: (state) => {
      state.activeInstances = [];
    },
  },
});

export const {
  setSkills,
  selectSkill,
  addSkillInstance,
  removeSkillInstance,
  clearSkillInstances,
} = skillSlice.actions;

export default skillSlice.reducer;
```

- [ ] **步骤 3：创建 channelSlice.ts**

```typescript
// ui/clawteam-chat/src/store/channelSlice.ts
import { createSlice, type PayloadAction } from "@reduxjs/toolkit";
import type { ChannelConfig, ChannelStatus } from "@/types/channel";

interface ChannelSliceState {
  configs: Record<string, ChannelConfig>;
  statuses: Record<string, ChannelStatus>;
}

const initialState: ChannelSliceState = {
  configs: {},
  statuses: {},
};

const channelSlice = createSlice({
  name: "channel",
  initialState,
  reducers: {
    setChannelConfigs: (state, action: PayloadAction<ChannelConfig[]>) => {
      state.configs = {};
      for (const config of action.payload) {
        state.configs[config.type] = config;
      }
    },
    updateChannelConfig: (
      state,
      action: PayloadAction<{ type: string; config: Partial<ChannelConfig> }>
    ) => {
      const existing = state.configs[action.payload.type];
      if (existing) {
        state.configs[action.payload.type] = { ...existing, ...action.payload.config };
      }
    },
    setChannelStatuses: (state, action: PayloadAction<ChannelStatus[]>) => {
      state.statuses = {};
      for (const status of action.payload) {
        state.statuses[status.type] = status;
      }
    },
    updateChannelStatus: (
      state,
      action: PayloadAction<{ type: string; status: Partial<ChannelStatus> }>
    ) => {
      const existing = state.statuses[action.payload.type];
      if (existing) {
        state.statuses[action.payload.type] = { ...existing, ...action.payload.status };
      }
    },
  },
});

export const {
  setChannelConfigs,
  updateChannelConfig,
  setChannelStatuses,
  updateChannelStatus,
} = channelSlice.actions;

export default channelSlice.reducer;
```

- [ ] **步骤 4：更新 store/index.ts**

```typescript
// ui/clawteam-chat/src/store/index.ts
import { configureStore } from "@reduxjs/toolkit";
import chatReducer from "./chatSlice";
import agentReducer from "./agentSlice";
import skillReducer from "./skillSlice";
import channelReducer from "./channelSlice";

export const store = configureStore({
  reducer: {
    chat: chatReducer,
    agent: agentReducer,
    skill: skillReducer,
    channel: channelReducer,
  },
});

export type RootState = ReturnType<typeof store.getState>;
export type AppDispatch = typeof store.dispatch;
```

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/store/
git commit -m "feat(store): enhance agent slice and add skill/channel slices"
```

---

## 任务 4：Agent 面板组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/agent/AgentPanel.tsx`
- 创建：`ui/clawteam-chat/src/components/agent/AgentCard.tsx`
- 创建：`ui/clawteam-chat/src/components/agent/AgentStatusBadge.tsx`
- 创建：`ui/clawteam-chat/src/components/agent/AgentConfigModal.tsx`

- [ ] **步骤 1：创建 AgentStatusBadge.tsx**

```tsx
// ui/clawteam-chat/src/components/agent/AgentStatusBadge.tsx
import { Badge } from "@/components/ui/badge";
import type { TerminalSessionStatus } from "@/types/agent";

interface AgentStatusBadgeProps {
  status: TerminalSessionStatus;
}

const statusConfig: Record<TerminalSessionStatus, { label: string; variant: "default" | "secondary" | "destructive" | "outline" | "success" | "warning" | "working" }> = {
  pending: { label: "启动中", variant: "warning" },
  online: { label: "在线", variant: "success" },
  working: { label: "执行中", variant: "working" },
  offline: { label: "离线", variant: "secondary" },
  broken: { label: "异常", variant: "destructive" },
};

export function AgentStatusBadge({ status }: AgentStatusBadgeProps) {
  const config = statusConfig[status];
  return <Badge variant={config.variant}>{config.label}</Badge>;
}
```

- [ ] **步骤 2：创建 AgentCard.tsx**

```tsx
// ui/clawteam-chat/src/components/agent/AgentCard.tsx
import { Bot, Wrench, Eye, MoreVertical, Play, Square, Settings } from "lucide-react";
import { cn } from "@/lib/utils";
import type { AgentState, AgentRole } from "@/types/agent";
import { ROLE_COLORS, STATUS_COLORS } from "@/types/agent";
import { AgentStatusBadge } from "./AgentStatusBadge";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";
import { Button } from "@/components/ui/button";

interface AgentCardProps {
  agent: AgentState;
  isActive?: boolean;
  onSelect?: () => void;
  onStart?: () => void;
  onStop?: () => void;
  onConfig?: () => void;
}

const RoleIcon = ({ role }: { role: AgentRole }) => {
  const icons: Record<AgentRole, React.ReactNode> = {
    assistant: <Bot className="h-4 w-4" />,
    worker: <Wrench className="h-4 w-4" />,
    supervisor: <Eye className="h-4 w-4" />,
  };
  return icons[role];
};

const roleLabels: Record<AgentRole, string> = {
  assistant: "助手",
  worker: "员工",
  supervisor: "监工",
};

export function AgentCard({
  agent,
  isActive,
  onSelect,
  onStart,
  onStop,
  onConfig,
}: AgentCardProps) {
  const { config, status } = agent;
  const roleColors = ROLE_COLORS[config.role];
  const statusColor = STATUS_COLORS[status];
  const isOnline = status === "online" || status === "working";

  return (
    <div
      className={cn(
        "group relative flex items-center gap-3 rounded-lg p-3 cursor-pointer transition-colors",
        isActive ? "bg-primary/10 border border-primary/30" : "hover:bg-white/5 border border-transparent"
      )}
      onClick={onSelect}
    >
      {/* 状态指示灯 */}
      <div className="relative">
        <div
          className={cn(
            "flex h-10 w-10 items-center justify-center rounded-lg",
            roleColors.bg,
            roleColors.text,
            "ring-1",
            roleColors.ring
          )}
        >
          <RoleIcon role={config.role} />
        </div>
        <span
          className={cn(
            "absolute -bottom-0.5 -right-0.5 h-3 w-3 rounded-full ring-2 ring-background",
            statusColor
          )}
        />
      </div>

      {/* Agent 信息 */}
      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-2">
          <span className="font-medium text-sm truncate">{config.name}</span>
          <AgentStatusBadge status={status} />
        </div>
        <div className="flex items-center gap-2 text-xs text-muted-foreground">
          <span>{roleLabels[config.role]}</span>
          <span>•</span>
          <span>{config.cliTool}</span>
        </div>
      </div>

      {/* 操作菜单 */}
      <DropdownMenu>
        <DropdownMenuTrigger asChild>
          <Button
            variant="ghost"
            size="icon"
            className="h-8 w-8 opacity-0 group-hover:opacity-100 transition-opacity"
            onClick={(e) => e.stopPropagation()}
          >
            <MoreVertical className="h-4 w-4" />
          </Button>
        </DropdownMenuTrigger>
        <DropdownMenuContent align="end">
          {isOnline ? (
            <DropdownMenuItem onClick={(e) => { e.stopPropagation(); onStop?.(); }}>
              <Square className="mr-2 h-4 w-4" />
              停止
            </DropdownMenuItem>
          ) : (
            <DropdownMenuItem onClick={(e) => { e.stopPropagation(); onStart?.(); }}>
              <Play className="mr-2 h-4 w-4" />
              启动
            </DropdownMenuItem>
          )}
          <DropdownMenuItem onClick={(e) => { e.stopPropagation(); onConfig?.(); }}>
            <Settings className="mr-2 h-4 w-4" />
            配置
          </DropdownMenuItem>
        </DropdownMenuContent>
      </DropdownMenu>
    </div>
  );
}
```

- [ ] **步骤 3：创建 AgentPanel.tsx**

```tsx
// ui/clawteam-chat/src/components/agent/AgentPanel.tsx
import { Plus, Bot, Wrench, Eye } from "lucide-react";
import { useAppDispatch, useAppSelector } from "@/hooks/useAppDispatch";
import { setActiveAgent, addAgent, removeAgent } from "@/store/agentSlice";
import { AgentCard } from "./AgentCard";
import { Button } from "@/components/ui/button";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import type { AgentRole, AgentConfig } from "@/types/agent";
import { useState } from "react";
import { AgentConfigModal } from "./AgentConfigModal";

export function AgentPanel() {
  const dispatch = useAppDispatch();
  const { agents, agentList, activeAgentId } = useAppSelector((s) => s.agent);
  const [configModalOpen, setConfigModalOpen] = useState(false);
  const [editingAgent, setEditingAgent] = useState<AgentConfig | null>(null);

  const handleSelect = (id: string) => {
    dispatch(setActiveAgent(id));
  };

  const handleStart = (id: string) => {
    // TODO: Call API to start agent
    console.log("Start agent:", id);
  };

  const handleStop = (id: string) => {
    // TODO: Call API to stop agent
    console.log("Stop agent:", id);
  };

  const handleConfig = (id: string) => {
    const agent = agents[id];
    if (agent) {
      setEditingAgent(agent.config);
      setConfigModalOpen(true);
    }
  };

  const handleAddAgent = (role: AgentRole) => {
    const newAgent: AgentConfig = {
      id: `${role}-${Date.now()}`,
      name: `新${role === "assistant" ? "助手" : role === "worker" ? "员工" : "监工"}`,
      role,
      cliTool: "opencode",
      args: [],
      env: {},
      autoStart: false,
      enabled: true,
      skills: [],
    };
    setEditingAgent(newAgent);
    setConfigModalOpen(true);
  };

  const handleSaveConfig = (config: AgentConfig) => {
    if (agentList.includes(config.id)) {
      // Update existing
      // dispatch(updateAgent({ id: config.id, config }));
    } else {
      dispatch(addAgent(config));
    }
    setConfigModalOpen(false);
    setEditingAgent(null);
  };

  // 按角色分组
  const groupedAgents = {
    assistant: agentList.filter((id) => agents[id]?.config.role === "assistant"),
    worker: agentList.filter((id) => agents[id]?.config.role === "worker"),
    supervisor: agentList.filter((id) => agents[id]?.config.role === "supervisor"),
  };

  return (
    <div className="flex flex-col h-full w-64 border-r bg-background">
      <div className="p-4 border-b">
        <h2 className="font-semibold">Agent 面板</h2>
      </div>

      <Tabs defaultValue="all" className="flex-1 flex flex-col">
        <TabsList className="mx-4 mt-2">
          <TabsTrigger value="all" className="flex-1">全部</TabsTrigger>
          <TabsTrigger value="assistant" className="flex-1">助手</TabsTrigger>
          <TabsTrigger value="worker" className="flex-1">员工</TabsTrigger>
        </TabsList>

        <TabsContent value="all" className="flex-1 m-0">
          <ScrollArea className="h-full">
            <div className="p-2 space-y-1">
              {agentList.map((id) => (
                <AgentCard
                  key={id}
                  agent={agents[id]}
                  isActive={activeAgentId === id}
                  onSelect={() => handleSelect(id)}
                  onStart={() => handleStart(id)}
                  onStop={() => handleStop(id)}
                  onConfig={() => handleConfig(id)}
                />
              ))}
            </div>
          </ScrollArea>
        </TabsContent>

        <TabsContent value="assistant" className="flex-1 m-0">
          <ScrollArea className="h-full">
            <div className="p-2 space-y-1">
              {groupedAgents.assistant.map((id) => (
                <AgentCard
                  key={id}
                  agent={agents[id]}
                  isActive={activeAgentId === id}
                  onSelect={() => handleSelect(id)}
                  onStart={() => handleStart(id)}
                  onStop={() => handleStop(id)}
                  onConfig={() => handleConfig(id)}
                />
              ))}
            </div>
          </ScrollArea>
        </TabsContent>

        <TabsContent value="worker" className="flex-1 m-0">
          <ScrollArea className="h-full">
            <div className="p-2 space-y-1">
              {[...groupedAgents.worker, ...groupedAgents.supervisor].map((id) => (
                <AgentCard
                  key={id}
                  agent={agents[id]}
                  isActive={activeAgentId === id}
                  onSelect={() => handleSelect(id)}
                  onStart={() => handleStart(id)}
                  onStop={() => handleStop(id)}
                  onConfig={() => handleConfig(id)}
                />
              ))}
            </div>
          </ScrollArea>
        </TabsContent>
      </Tabs>

      {/* 添加按钮组 */}
      <div className="p-4 border-t space-y-2">
        <Button
          variant="outline"
          className="w-full justify-start"
          onClick={() => handleAddAgent("assistant")}
        >
          <Bot className="mr-2 h-4 w-4" />
          添加助手
        </Button>
        <Button
          variant="outline"
          className="w-full justify-start"
          onClick={() => handleAddAgent("worker")}
        >
          <Wrench className="mr-2 h-4 w-4" />
          添加员工
        </Button>
        <Button
          variant="outline"
          className="w-full justify-start"
          onClick={() => handleAddAgent("supervisor")}
        >
          <Eye className="mr-2 h-4 w-4" />
          添加监工
        </Button>
      </div>

      <AgentConfigModal
        open={configModalOpen}
        onOpenChange={setConfigModalOpen}
        agent={editingAgent}
        onSave={handleSaveConfig}
      />
    </div>
  );
}
```

- [ ] **步骤 4：创建 AgentConfigModal.tsx**

```tsx
// ui/clawteam-chat/src/components/agent/AgentConfigModal.tsx
import { useState } from "react";
import type { AgentConfig, AgentRole } from "@/types/agent";
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogFooter,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { Switch } from "@/components/ui/switch";

interface AgentConfigModalProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  agent: AgentConfig | null;
  onSave: (config: AgentConfig) => void;
}

const CLI_TOOLS = [
  { value: "claude", label: "Claude Code" },
  { value: "codex", label: "Codex" },
  { value: "gemini", label: "Gemini" },
  { value: "opencode", label: "OpenCode" },
  { value: "qwen", label: "Qwen" },
  { value: "shell", label: "Shell" },
];

const roleLabels: Record<AgentRole, string> = {
  assistant: "助手",
  worker: "员工",
  supervisor: "监工",
};

export function AgentConfigModal({
  open,
  onOpenChange,
  agent,
  onSave,
}: AgentConfigModalProps) {
  const [config, setConfig] = useState<AgentConfig>(
    agent ?? {
      id: "",
      name: "",
      role: "worker",
      cliTool: "opencode",
      args: [],
      env: {},
      autoStart: false,
      enabled: true,
      skills: [],
    }
  );

  const handleSave = () => {
    onSave(config);
  };

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="sm:max-w-[500px]">
        <DialogHeader>
          <DialogTitle>
            {agent ? `配置 ${agent.name}` : "添加 Agent"}
          </DialogTitle>
        </DialogHeader>

        <div className="grid gap-4 py-4">
          <div className="grid grid-cols-4 items-center gap-4">
            <Label htmlFor="name" className="text-right">
              名称
            </Label>
            <Input
              id="name"
              value={config.name}
              onChange={(e) => setConfig({ ...config, name: e.target.value })}
              className="col-span-3"
            />
          </div>

          <div className="grid grid-cols-4 items-center gap-4">
            <Label htmlFor="role" className="text-right">
              角色
            </Label>
            <Select
              value={config.role}
              onValueChange={(value: AgentRole) =>
                setConfig({ ...config, role: value })
              }
            >
              <SelectTrigger className="col-span-3">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="assistant">助手（协调器）</SelectItem>
                <SelectItem value="worker">员工（执行者）</SelectItem>
                <SelectItem value="supervisor">监工（审核者）</SelectItem>
              </SelectContent>
            </Select>
          </div>

          <div className="grid grid-cols-4 items-center gap-4">
            <Label htmlFor="cliTool" className="text-right">
              CLI 工具
            </Label>
            <Select
              value={config.cliTool}
              onValueChange={(value) =>
                setConfig({ ...config, cliTool: value })
              }
            >
              <SelectTrigger className="col-span-3">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                {CLI_TOOLS.map((tool) => (
                  <SelectItem key={tool.value} value={tool.value}>
                    {tool.label}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </div>

          <div className="grid grid-cols-4 items-center gap-4">
            <Label htmlFor="cwd" className="text-right">
              工作目录
            </Label>
            <Input
              id="cwd"
              value={config.cwd ?? ""}
              onChange={(e) =>
                setConfig({ ...config, cwd: e.target.value || undefined })
              }
              placeholder="可选"
              className="col-span-3"
            />
          </div>

          <div className="grid grid-cols-4 items-center gap-4">
            <Label htmlFor="autoStart" className="text-right">
              自动启动
            </Label>
            <Switch
              id="autoStart"
              checked={config.autoStart}
              onCheckedChange={(checked) =>
                setConfig({ ...config, autoStart: checked })
              }
            />
          </div>

          <div className="grid grid-cols-4 items-center gap-4">
            <Label htmlFor="enabled" className="text-right">
              启用
            </Label>
            <Switch
              id="enabled"
              checked={config.enabled}
              onCheckedChange={(checked) =>
                setConfig({ ...config, enabled: checked })
              }
            />
          </div>
        </div>

        <DialogFooter>
          <Button variant="outline" onClick={() => onOpenChange(false)}>
            取消
          </Button>
          <Button onClick={handleSave}>保存</Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  );
}
```

- [ ] **步骤 5：Commit**

```bash
git add ui/clawteam-chat/src/components/agent/
git commit -m "feat(agent): add AgentPanel, AgentCard, AgentStatusBadge, and AgentConfigModal components"
```

---

## 任务 5：Skill 组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/skill/SkillStore.tsx`
- 创建：`ui/clawteam-chat/src/components/skill/SkillCard.tsx`
- 创建：`ui/clawteam-chat/src/components/skill/SkillDetailModal.tsx`
- 创建：`ui/clawteam-chat/src/hooks/useSkills.ts`
- 创建：`ui/clawteam-chat/src/api/skill.ts`

- [ ] **步骤 1：创建 skill.ts API**

```typescript
// ui/clawteam-chat/src/api/skill.ts
import { apiGet } from "./client";
import type { Skill } from "@/types/skill";

export async function getSkills(): Promise<Skill[]> {
  return apiGet<Skill[]>("/skills");
}

export async function getSkill(id: string): Promise<Skill> {
  return apiGet<Skill>(`/skills/${id}`);
}
```

- [ ] **步骤 2：创建 useSkills.ts Hook**

```typescript
// ui/clawteam-chat/src/hooks/useSkills.ts
import { useCallback } from "react";
import { useAppDispatch, useAppSelector } from "./useAppDispatch";
import { setSkills, selectSkill, addSkillInstance } from "@/store/skillSlice";
import { getSkills as fetchSkills } from "@/api/skill";
import type { Skill, SkillInstance } from "@/types/skill";

export function useSkills() {
  const dispatch = useAppDispatch();
  const { skills, skillList, selectedSkillId, activeInstances } = useAppSelector(
    (s) => s.skill
  );

  const loadSkills = useCallback(async () => {
    const data = await fetchSkills();
    dispatch(setSkills(data));
  }, [dispatch]);

  const selectSkillById = useCallback(
    (id: string | null) => {
      dispatch(selectSkill(id));
    },
    [dispatch]
  );

  const executeSkill = useCallback(
    (skillId: string, variables: Record<string, string>) => {
      const instance: SkillInstance = {
        skillId,
        variables,
        createdAt: Date.now(),
      };
      dispatch(addSkillInstance(instance));
    },
    [dispatch]
  );

  return {
    skills,
    skillList,
    selectedSkillId,
    activeInstances,
    selectedSkill: selectedSkillId ? skills[selectedSkillId] : null,
    loadSkills,
    selectSkill: selectSkillById,
    executeSkill,
  };
}
```

- [ ] **步骤 3：创建 SkillCard.tsx**

```tsx
// ui/clawteam-chat/src/components/skill/SkillCard.tsx
import { cn } from "@/lib/utils";
import type { Skill } from "@/types/skill";
import * as LucideIcons from "lucide-react";

interface SkillCardProps {
  skill: Skill;
  isSelected?: boolean;
  onClick?: () => void;
}

// 动态获取图标组件
const DynamicIcon = ({ name }: { name: string }) => {
  const IconComponent = (LucideIcons as Record<string, React.ComponentType<{ className?: string }>>)[
    name.charAt(0).toUpperCase() + name.slice(1)
  ];
  if (!IconComponent) {
    return <LucideIcons.Code className="h-5 w-5" />;
  }
  return <IconComponent className="h-5 w-5" />;
};

export function SkillCard({ skill, isSelected, onClick }: SkillCardProps) {
  return (
    <div
      className={cn(
        "relative flex flex-col items-center justify-center p-4 rounded-lg cursor-pointer transition-all",
        "border",
        isSelected
          ? "bg-primary/10 border-primary/30"
          : "bg-card hover:bg-accent border-border"
      )}
      onClick={onClick}
    >
      <div
        className={cn(
          "flex h-12 w-12 items-center justify-center rounded-lg mb-2",
          skill.bg,
          "ring-1",
          skill.ring
        )}
        style={{ color: skill.color }}
      >
        <DynamicIcon name={skill.icon} />
      </div>
      <span className="text-sm font-medium text-center">{skill.name}</span>
      <span className="text-xs text-muted-foreground mt-1">v{skill.version}</span>
    </div>
  );
}
```

- [ ] **步骤 4：创建 SkillStore.tsx**

```tsx
// ui/clawteam-chat/src/components/skill/SkillStore.tsx
import { useState } from "react";
import { useSkills } from "@/hooks/useSkills";
import { SkillCard } from "./SkillCard";
import { SkillDetailModal } from "./SkillDetailModal";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Input } from "@/components/ui/input";
import { Search, Package } from "lucide-react";
import type { Skill } from "@/types/skill";

interface SkillStoreProps {
  onExecute?: (skill: Skill, variables: Record<string, string>) => void;
}

export function SkillStore({ onExecute }: SkillStoreProps) {
  const { skills, skillList, selectSkill, selectedSkill } = useSkills();
  const [searchQuery, setSearchQuery] = useState("");
  const [detailOpen, setDetailOpen] = useState(false);

  const filteredSkills = skillList.filter((id) => {
    const skill = skills[id];
    if (!skill) return false;
    const query = searchQuery.toLowerCase();
    return (
      skill.name.toLowerCase().includes(query) ||
      skill.description.toLowerCase().includes(query) ||
      skill.tags.some((tag) => tag.toLowerCase().includes(query))
    );
  });

  const handleSkillClick = (skillId: string) => {
    selectSkill(skillId);
    setDetailOpen(true);
  };

  const handleExecute = (skill: Skill, variables: Record<string, string>) => {
    onExecute?.(skill, variables);
    setDetailOpen(false);
  };

  return (
    <div className="flex flex-col h-full">
      <div className="p-4 border-b">
        <h2 className="font-semibold flex items-center gap-2">
          <Package className="h-5 w-5" />
          Skill 商店
        </h2>
        <div className="mt-3 relative">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 h-4 w-4 text-muted-foreground" />
          <Input
            placeholder="搜索技能..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="pl-9"
          />
        </div>
      </div>

      <ScrollArea className="flex-1">
        <div className="p-4 grid grid-cols-2 gap-3">
          {filteredSkills.map((id) => (
            <SkillCard
              key={id}
              skill={skills[id]}
              isSelected={selectedSkill?.id === id}
              onClick={() => handleSkillClick(id)}
            />
          ))}
          {filteredSkills.length === 0 && (
            <div className="col-span-2 text-center text-muted-foreground py-8">
              未找到匹配的技能
            </div>
          )}
        </div>
      </ScrollArea>

      <SkillDetailModal
        open={detailOpen}
        onOpenChange={setDetailOpen}
        skill={selectedSkill}
        onExecute={handleExecute}
      />
    </div>
  );
}
```

- [ ] **步骤 5：创建 SkillDetailModal.tsx**

```tsx
// ui/clawteam-chat/src/components/skill/SkillDetailModal.tsx
import { useState } from "react";
import type { Skill, SkillVariable } from "@/types/skill";
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogDescription,
  DialogFooter,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import * as LucideIcons from "lucide-react";

interface SkillDetailModalProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  skill: Skill | null;
  onExecute?: (skill: Skill, variables: Record<string, string>) => void;
}

const DynamicIcon = ({ name }: { name: string }) => {
  const IconComponent = (LucideIcons as Record<string, React.ComponentType<{ className?: string }>>)[
    name.charAt(0).toUpperCase() + name.slice(1)
  ];
  if (!IconComponent) return null;
  return <IconComponent className="h-6 w-6" />;
};

export function SkillDetailModal({
  open,
  onOpenChange,
  skill,
  onExecute,
}: SkillDetailModalProps) {
  const [variables, setVariables] = useState<Record<string, string>>({});

  if (!skill) return null;

  const handleVariableChange = (name: string, value: string) => {
    setVariables((prev) => ({ ...prev, [name]: value }));
  };

  const handleExecute = () => {
    onExecute?.(skill, variables);
    setVariables({});
  };

  const renderVariableInput = (variable: SkillVariable) => {
    switch (variable.varType) {
      case "select":
        return (
          <Select
            value={variables[variable.name] ?? variable.default ?? ""}
            onValueChange={(value) => handleVariableChange(variable.name, value)}
          >
            <SelectTrigger>
              <SelectValue placeholder={variable.label} />
            </SelectTrigger>
            <SelectContent>
              {variable.options?.map((option) => (
                <SelectItem key={option} value={option}>
                  {option}
                </SelectItem>
              ))}
            </SelectContent>
          </Select>
        );
      case "number":
        return (
          <Input
            type="number"
            value={variables[variable.name] ?? variable.default ?? ""}
            onChange={(e) => handleVariableChange(variable.name, e.target.value)}
            placeholder={variable.label}
          />
        );
      default:
        return (
          <Input
            value={variables[variable.name] ?? variable.default ?? ""}
            onChange={(e) => handleVariableChange(variable.name, e.target.value)}
            placeholder={variable.label}
          />
        );
    }
  };

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="sm:max-w-[500px]">
        <DialogHeader>
          <div className="flex items-center gap-3">
            <div
              className="flex h-10 w-10 items-center justify-center rounded-lg"
              style={{ backgroundColor: skill.bg, color: skill.color }}
            >
              <DynamicIcon name={skill.icon} />
            </div>
            <div>
              <DialogTitle>{skill.name}</DialogTitle>
              <DialogDescription>v{skill.version}</DialogDescription>
            </div>
          </div>
        </DialogHeader>

        <div className="py-4">
          <p className="text-sm text-muted-foreground mb-4">
            {skill.description}
          </p>

          {skill.template.variables.length > 0 && (
            <div className="space-y-4">
              <h4 className="font-medium text-sm">参数</h4>
              {skill.template.variables.map((variable) => (
                <div key={variable.name} className="grid grid-cols-4 items-center gap-4">
                  <Label htmlFor={variable.name} className="text-right">
                    {variable.label}
                    {variable.required && <span className="text-destructive ml-1">*</span>}
                  </Label>
                  <div className="col-span-3">{renderVariableInput(variable)}</div>
                </div>
              ))}
            </div>
          )}
        </div>

        <DialogFooter>
          <Button variant="outline" onClick={() => onOpenChange(false)}>
            取消
          </Button>
          <Button onClick={handleExecute}>执行</Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  );
}
```

- [ ] **步骤 6：Commit**

```bash
git add ui/clawteam-chat/src/components/skill/ ui/clawteam-chat/src/hooks/useSkills.ts ui/clawteam-chat/src/api/skill.ts
git commit -m "feat(skill): add SkillStore, SkillCard, SkillDetailModal components and hooks"
```

---

## 任务 6：渠道配置组件

**文件：**
- 创建：`ui/clawteam-chat/src/components/channel/ChannelConfig.tsx`
- 创建：`ui/clawteam-chat/src/components/channel/FeishuConfig.tsx`
- 创建：`ui/clawteam-chat/src/components/channel/WeixinConfig.tsx`
- 创建：`ui/clawteam-chat/src/hooks/useChannels.ts`
- 创建：`ui/clawteam-chat/src/api/channel.ts`

- [ ] **步骤 1：创建 channel.ts API**

```typescript
// ui/clawteam-chat/src/api/channel.ts
import { apiGet, apiPost } from "./client";
import type { ChannelConfig, ChannelStatus } from "@/types/channel";

export async function getChannelConfigs(): Promise<ChannelConfig[]> {
  return apiGet<ChannelConfig[]>("/channels");
}

export async function updateChannelConfig(
  type: string,
  config: Partial<ChannelConfig>
): Promise<ChannelConfig> {
  return apiPost<ChannelConfig>(`/channels/${type}`, config);
}

export async function getChannelStatuses(): Promise<ChannelStatus[]> {
  return apiGet<ChannelStatus[]>("/channels/status");
}
```

- [ ] **步骤 2：创建 useChannels.ts Hook**

```typescript
// ui/clawteam-chat/src/hooks/useChannels.ts
import { useCallback } from "react";
import { useAppDispatch, useAppSelector } from "./useAppDispatch";
import {
  setChannelConfigs,
  updateChannelConfig,
  setChannelStatuses,
} from "@/store/channelSlice";
import {
  getChannelConfigs as fetchConfigs,
  updateChannelConfig as updateConfig,
  getChannelStatuses as fetchStatuses,
} from "@/api/channel";
import type { ChannelConfig } from "@/types/channel";

export function useChannels() {
  const dispatch = useAppDispatch();
  const { configs, statuses } = useAppSelector((s) => s.channel);

  const loadConfigs = useCallback(async () => {
    const data = await fetchConfigs();
    dispatch(setChannelConfigs(data));
  }, [dispatch]);

  const updateConfigByType = useCallback(
    async (type: string, config: Partial<ChannelConfig>) => {
      const updated = await updateConfig(type, config);
      dispatch(updateChannelConfig({ type, config: updated }));
    },
    [dispatch]
  );

  const loadStatuses = useCallback(async () => {
    const data = await fetchStatuses();
    dispatch(setChannelStatuses(data));
  }, [dispatch]);

  return {
    configs,
    statuses,
    feishuConfig: configs["feishu"],
    weixinConfig: configs["weixin"],
    loadConfigs,
    updateConfig: updateConfigByType,
    loadStatuses,
  };
}
```

- [ ] **步骤 3：创建 FeishuConfig.tsx**

```tsx
// ui/clawteam-chat/src/components/channel/FeishuConfig.tsx
import { useState } from "react";
import { useChannels } from "@/hooks/useChannels";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Switch } from "@/components/ui/switch";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { MessageCircle, ExternalLink } from "lucide-react";

export function FeishuConfig() {
  const { feishuConfig, updateConfig, statuses } = useChannels();
  const [appId, setAppId] = useState(feishuConfig?.appId ?? "");
  const [appSecret, setAppSecret] = useState(feishuConfig?.appSecret ?? "");
  const [encryptKey, setEncryptKey] = useState(feishuConfig?.encryptKey ?? "");
  const [enabled, setEnabled] = useState(feishuConfig?.enabled ?? false);

  const status = statuses["feishu"];

  const handleSave = async () => {
    await updateConfig("feishu", {
      appId,
      appSecret,
      encryptKey,
      enabled,
    });
  };

  return (
    <Card>
      <CardHeader>
        <div className="flex items-center justify-between">
          <CardTitle className="flex items-center gap-2">
            <MessageCircle className="h-5 w-5" />
            飞书
          </CardTitle>
          <div className="flex items-center gap-2">
            {status?.connected ? (
              <Badge variant="success">已连接</Badge>
            ) : (
              <Badge variant="secondary">未连接</Badge>
            )}
            <Switch
              checked={enabled}
              onCheckedChange={setEnabled}
            />
          </div>
        </div>
      </CardHeader>
      <CardContent className="space-y-4">
        <div className="grid gap-2">
          <Label htmlFor="feishu-appId">App ID</Label>
          <Input
            id="feishu-appId"
            value={appId}
            onChange={(e) => setAppId(e.target.value)}
            placeholder="cli_xxxxxxxxxx"
          />
        </div>

        <div className="grid gap-2">
          <Label htmlFor="feishu-appSecret">App Secret</Label>
          <Input
            id="feishu-appSecret"
            type="password"
            value={appSecret}
            onChange={(e) => setAppSecret(e.target.value)}
            placeholder="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
          />
        </div>

        <div className="grid gap-2">
          <Label htmlFor="feishu-encryptKey">Encrypt Key（可选）</Label>
          <Input
            id="feishu-encryptKey"
            value={encryptKey}
            onChange={(e) => setEncryptKey(e.target.value)}
            placeholder="用于消息加密"
          />
        </div>

        <div className="flex justify-between items-center pt-4">
          <a
            href="https://open.feishu.cn/app"
            target="_blank"
            rel="noopener noreferrer"
            className="text-sm text-primary hover:underline flex items-center gap-1"
          >
            飞书开放平台
            <ExternalLink className="h-3 w-3" />
          </a>
          <Button onClick={handleSave}>保存配置</Button>
        </div>
      </CardContent>
    </Card>
  );
}
```

- [ ] **步骤 4：创建 WeixinConfig.tsx**

```tsx
// ui/clawteam-chat/src/components/channel/WeixinConfig.tsx
import { useState } from "react";
import { useChannels } from "@/hooks/useChannels";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Switch } from "@/components/ui/switch";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { MessageSquare, ExternalLink } from "lucide-react";

export function WeixinConfig() {
  const { weixinConfig, updateConfig, statuses } = useChannels();
  const [appId, setAppId] = useState(weixinConfig?.appId ?? "");
  const [appSecret, setAppSecret] = useState(weixinConfig?.appSecret ?? "");
  const [token, setToken] = useState(weixinConfig?.token ?? "");
  const [enabled, setEnabled] = useState(weixinConfig?.enabled ?? false);

  const status = statuses["weixin"];

  const handleSave = async () => {
    await updateConfig("weixin", {
      appId,
      appSecret,
      token,
      enabled,
    });
  };

  return (
    <Card>
      <CardHeader>
        <div className="flex items-center justify-between">
          <CardTitle className="flex items-center gap-2">
            <MessageSquare className="h-5 w-5" />
            微信公众号
          </CardTitle>
          <div className="flex items-center gap-2">
            {status?.connected ? (
              <Badge variant="success">已连接</Badge>
            ) : (
              <Badge variant="secondary">未连接</Badge>
            )}
            <Switch
              checked={enabled}
              onCheckedChange={setEnabled}
            />
          </div>
        </div>
      </CardHeader>
      <CardContent className="space-y-4">
        <div className="grid gap-2">
          <Label htmlFor="weixin-appId">App ID</Label>
          <Input
            id="weixin-appId"
            value={appId}
            onChange={(e) => setAppId(e.target.value)}
            placeholder="wxXXXXXXXXXXXXXXXX"
          />
        </div>

        <div className="grid gap-2">
          <Label htmlFor="weixin-appSecret">App Secret</Label>
          <Input
            id="weixin-appSecret"
            type="password"
            value={appSecret}
            onChange={(e) => setAppSecret(e.target.value)}
            placeholder="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
          />
        </div>

        <div className="grid gap-2">
          <Label htmlFor="weixin-token">Token</Label>
          <Input
            id="weixin-token"
            value={token}
            onChange={(e) => setToken(e.target.value)}
            placeholder="服务器配置 Token"
          />
        </div>

        <div className="flex justify-between items-center pt-4">
          <a
            href="https://mp.weixin.qq.com"
            target="_blank"
            rel="noopener noreferrer"
            className="text-sm text-primary hover:underline flex items-center gap-1"
          >
            微信公众平台
            <ExternalLink className="h-3 w-3" />
          </a>
          <Button onClick={handleSave}>保存配置</Button>
        </div>
      </CardContent>
    </Card>
  );
}
```

- [ ] **步骤 5：创建 ChannelConfig.tsx**

```tsx
// ui/clawteam-chat/src/components/channel/ChannelConfig.tsx
import { useEffect } from "react";
import { useChannels } from "@/hooks/useChannels";
import { FeishuConfig } from "./FeishuConfig";
import { WeixinConfig } from "./WeixinConfig";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Radio, MessageCircle, MessageSquare } from "lucide-react";

export function ChannelConfig() {
  const { loadConfigs, loadStatuses } = useChannels();

  useEffect(() => {
    loadConfigs();
    loadStatuses();
  }, [loadConfigs, loadStatuses]);

  return (
    <div className="flex flex-col h-full">
      <div className="p-4 border-b">
        <h2 className="font-semibold flex items-center gap-2">
          <Radio className="h-5 w-5" />
          渠道配置
        </h2>
        <p className="text-sm text-muted-foreground mt-1">
          配置消息接入渠道，支持飞书、微信公众号等平台
        </p>
      </div>

      <Tabs defaultValue="feishu" className="flex-1 flex flex-col">
        <TabsList className="mx-4 mt-2">
          <TabsTrigger value="feishu" className="flex items-center gap-2">
            <MessageCircle className="h-4 w-4" />
            飞书
          </TabsTrigger>
          <TabsTrigger value="weixin" className="flex items-center gap-2">
            <MessageSquare className="h-4 w-4" />
            微信
          </TabsTrigger>
        </TabsList>

        <TabsContent value="feishu" className="flex-1 m-0">
          <ScrollArea className="h-full p-4">
            <FeishuConfig />
          </ScrollArea>
        </TabsContent>

        <TabsContent value="weixin" className="flex-1 m-0">
          <ScrollArea className="h-full p-4">
            <WeixinConfig />
          </ScrollArea>
        </TabsContent>
      </Tabs>
    </div>
  );
}
```

- [ ] **步骤 6：Commit**

```bash
git add ui/clawteam-chat/src/components/channel/ ui/clawteam-chat/src/hooks/useChannels.ts ui/clawteam-chat/src/api/channel.ts
git commit -m "feat(channel): add Feishu and Weixin channel configuration components"
```

---

## 任务 7：增强聊天界面

**文件：**
- 修改：`ui/clawteam-chat/src/features/chat/ChatInterface.tsx`
- 创建：`ui/clawteam-chat/src/features/chat/ChatHeader.tsx`
- 创建：`ui/clawteam-chat/src/features/chat/MessageList.tsx`
- 创建：`ui/clawteam-chat/src/features/chat/ChatInput.tsx`
- 创建：`ui/clawteam-chat/src/features/chat/MessageBubble.tsx`

- [ ] **步骤 1：创建 MessageBubble.tsx**

```tsx
// ui/clawteam-chat/src/features/chat/MessageBubble.tsx
import { cn } from "@/lib/utils";
import type { ChatMessage, SenderType } from "@/types/message";
import { Avatar, AvatarFallback } from "@/components/ui/avatar";
import {
  Bot,
  User,
  Wrench,
  Eye,
  AlertCircle,
} from "lucide-react";

interface MessageBubbleProps {
  message: ChatMessage;
  isCurrentUser?: boolean;
}

const senderIcons: Record<SenderType, React.ReactNode> = {
  user: <User className="h-4 w-4" />,
  assistant: <Bot className="h-4 w-4" />,
  worker: <Wrench className="h-4 w-4" />,
  supervisor: <Eye className="h-4 w-4" />,
  system: <AlertCircle className="h-4 w-4" />,
};

const senderColors: Record<SenderType, string> = {
  user: "bg-primary text-primary-foreground",
  assistant: "bg-purple-500/20 text-purple-400",
  worker: "bg-blue-500/20 text-blue-400",
  supervisor: "bg-green-500/20 text-green-400",
  system: "bg-muted text-muted-foreground",
};

export function MessageBubble({ message, isCurrentUser }: MessageBubbleProps) {
  const isUser = message.senderType === "user";

  return (
    <div
      className={cn(
        "flex gap-3 group",
        isUser && "flex-row-reverse"
      )}
    >
      <Avatar className="h-8 w-8 shrink-0">
        <AvatarFallback
          className={cn(
            "text-xs",
            senderColors[message.senderType]
          )}
        >
          {senderIcons[message.senderType]}
        </AvatarFallback>
      </Avatar>

      <div
        className={cn(
          "flex-1 rounded-lg px-4 py-2 max-w-[80%]",
          isUser
            ? "bg-primary text-primary-foreground ml-2"
            : "bg-muted mr-2"
        )}
      >
        {!isUser && (
          <div className="flex items-center gap-2 mb-1">
            <span className="text-xs font-medium">{message.senderName}</span>
            {message.linkedAgent && (
              <span className="text-xs text-muted-foreground">
                → {message.linkedAgent}
              </span>
            )}
          </div>
        )}
        <div className="text-sm whitespace-pre-wrap">{message.content}</div>
      </div>
    </div>
  );
}
```

- [ ] **步骤 2：创建 ChatHeader.tsx**

```tsx
// ui/clawteam-chat/src/features/chat/ChatHeader.tsx
import { Button } from "@/components/ui/button";
import { Settings, Package, Radio } from "lucide-react";

interface ChatHeaderProps {
  title?: string;
  onOpenSettings?: () => void;
  onOpenSkillStore?: () => void;
  onOpenChannels?: () => void;
}

export function ChatHeader({
  title = "ClawTeam Chat",
  onOpenSettings,
  onOpenSkillStore,
  onOpenChannels,
}: ChatHeaderProps) {
  return (
    <div className="flex items-center justify-between px-4 py-3 border-b bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60">
      <div className="flex items-center gap-2">
        <h1 className="font-semibold">{title}</h1>
      </div>

      <div className="flex items-center gap-1">
        <Button
          variant="ghost"
          size="icon"
          onClick={onOpenSkillStore}
          title="Skill 商店"
        >
          <Package className="h-4 w-4" />
        </Button>
        <Button
          variant="ghost"
          size="icon"
          onClick={onOpenChannels}
          title="渠道配置"
        >
          <Radio className="h-4 w-4" />
        </Button>
        <Button
          variant="ghost"
          size="icon"
          onClick={onOpenSettings}
          title="设置"
        >
          <Settings className="h-4 w-4" />
        </Button>
      </div>
    </div>
  );
}
```

- [ ] **步骤 3：创建 MessageList.tsx**

```tsx
// ui/clawteam-chat/src/features/chat/MessageList.tsx
import { useRef, useEffect } from "react";
import { ScrollArea } from "@/components/ui/scroll-area";
import { MessageBubble } from "./MessageBubble";
import type { ChatMessage } from "@/types/message";
import { Bot } from "lucide-react";

interface MessageListProps {
  messages: ChatMessage[];
  isLoading?: boolean;
}

export function MessageList({ messages, isLoading }: MessageListProps) {
  const scrollRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [messages]);

  if (messages.length === 0 && !isLoading) {
    return (
      <div className="flex-1 flex items-center justify-center">
        <div className="text-center text-muted-foreground">
          <Bot className="mx-auto mb-4 h-12 w-12 opacity-50" />
          <p className="text-lg font-medium">ClawTeam Chat</p>
          <p className="text-sm">多智能体协作终端</p>
        </div>
      </div>
    );
  }

  return (
    <ScrollArea className="flex-1 p-4" ref={scrollRef}>
      <div className="mx-auto max-w-3xl space-y-4">
        {messages.map((msg) => (
          <MessageBubble key={msg.id} message={msg} />
        ))}
        {isLoading && (
          <div className="flex gap-3">
            <div className="flex h-8 w-8 items-center justify-center rounded-full bg-muted">
              <Bot className="h-4 w-4" />
            </div>
            <div className="flex-1 rounded-lg bg-muted px-4 py-2 mr-2">
              <div className="flex items-center gap-2">
                <div className="h-2 w-2 animate-bounce rounded-full bg-muted-foreground [animation-delay:0ms]" />
                <div className="h-2 w-2 animate-bounce rounded-full bg-muted-foreground [animation-delay:150ms]" />
                <div className="h-2 w-2 animate-bounce rounded-full bg-muted-foreground [animation-delay:300ms]" />
              </div>
            </div>
          </div>
        )}
      </div>
    </ScrollArea>
  );
}
```

- [ ] **步骤 4：创建 ChatInput.tsx**

```tsx
// ui/clawteam-chat/src/features/chat/ChatInput.tsx
import { useState } from "react";
import { Send, Package } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Textarea } from "@/components/ui/textarea";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";
import { useSkills } from "@/hooks/useSkills";

interface ChatInputProps {
  value: string;
  onChange: (value: string) => void;
  onSend: (content: string, skillId?: string) => void;
  disabled?: boolean;
  placeholder?: string;
}

export function ChatInput({
  value,
  onChange,
  onSend,
  disabled,
  placeholder = "输入消息... (Enter 发送, Shift+Enter 换行)",
}: ChatInputProps) {
  const [selectedSkillId, setSelectedSkillId] = useState<string | null>(null);
  const { skills, skillList, loadSkills } = useSkills();

  // Load skills on first render
  useState(() => {
    loadSkills();
  });

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === "Enter" && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  };

  const handleSend = () => {
    if (value.trim() && !disabled) {
      onSend(value.trim(), selectedSkillId ?? undefined);
      setSelectedSkillId(null);
    }
  };

  const handleSkillSelect = (skillId: string) => {
    setSelectedSkillId(skillId === selectedSkillId ? null : skillId);
  };

  const selectedSkill = selectedSkillId ? skills[selectedSkillId] : null;

  return (
    <div className="border-t p-4">
      <div className="mx-auto max-w-3xl">
        {selectedSkill && (
          <div className="mb-2 flex items-center gap-2 text-sm text-muted-foreground">
            <Package className="h-4 w-4" />
            <span>使用技能: {selectedSkill.name}</span>
            <button
              onClick={() => setSelectedSkillId(null)}
              className="text-primary hover:underline"
            >
              取消
            </button>
          </div>
        )}
        <div className="flex gap-2">
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <Button variant="outline" size="icon" disabled={disabled}>
                <Package className="h-4 w-4" />
              </Button>
            </DropdownMenuTrigger>
            <DropdownMenuContent align="start" className="w-48">
              {skillList.slice(0, 5).map((id) => (
                <DropdownMenuItem
                  key={id}
                  onClick={() => handleSkillSelect(id)}
                  className={selectedSkillId === id ? "bg-accent" : ""}
                >
                  {skills[id].name}
                </DropdownMenuItem>
              ))}
            </DropdownMenuContent>
          </DropdownMenu>

          <Textarea
            value={value}
            onChange={(e) => onChange(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder={placeholder}
            className="flex-1 min-h-[60px] max-h-[200px] resize-none"
            disabled={disabled}
          />

          <Button onClick={handleSend} disabled={!value.trim() || disabled}>
            <Send className="h-4 w-4" />
          </Button>
        </div>
      </div>
    </div>
  );
}
```

- [ ] **步骤 5：重构 ChatInterface.tsx**

```tsx
// ui/clawteam-chat/src/features/chat/ChatInterface.tsx
import { useState, useEffect } from "react";
import { useChat } from "@/hooks/useChat";
import { ChatHeader } from "./ChatHeader";
import { MessageList } from "./MessageList";
import { ChatInput } from "./ChatInput";
import { AgentPanel } from "@/components/agent/AgentPanel";
import { SkillStore } from "@/components/skill/SkillStore";
import { ChannelConfig } from "@/components/channel/ChannelConfig";
import { Sheet, SheetContent } from "@/components/ui/sheet";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import type { Skill } from "@/types/skill";

type RightPanelType = "skills" | "channels" | "settings" | null;

export function ChatInterface() {
  const {
    messages,
    inputText,
    isLoading,
    setInputText,
    sendMessage,
    setCurrentSession,
  } = useChat();

  const [rightPanel, setRightPanel] = useState<RightPanelType>(null);

  useEffect(() => {
    setCurrentSession("default");
  }, [setCurrentSession]);

  const handleSend = (content: string, skillId?: string) => {
    sendMessage(content);
    // TODO: Handle skill execution
  };

  const handleSkillExecute = (skill: Skill, variables: Record<string, string>) => {
    // TODO: Execute skill with variables
    console.log("Execute skill:", skill, variables);
  };

  return (
    <div className="flex h-screen w-full">
      {/* 左侧 Agent 面板 */}
      <AgentPanel />

      {/* 中间聊天区域 */}
      <div className="flex-1 flex flex-col min-w-0">
        <ChatHeader
          onOpenSkillStore={() => setRightPanel("skills")}
          onOpenChannels={() => setRightPanel("channels")}
          onOpenSettings={() => setRightPanel("settings")}
        />
        <MessageList messages={messages} isLoading={isLoading} />
        <ChatInput
          value={inputText}
          onChange={setInputText}
          onSend={handleSend}
          disabled={isLoading}
        />
      </div>

      {/* 右侧面板 (Sheet) */}
      <Sheet open={rightPanel !== null} onOpenChange={(open) => !open && setRightPanel(null)}>
        <SheetContent className="w-[400px] sm:max-w-[400px] p-0">
          <Tabs
            value={rightPanel ?? "skills"}
            onValueChange={(v) => setRightPanel(v as RightPanelType)}
            className="h-full flex flex-col"
          >
            <TabsList className="mx-4 mt-4">
              <TabsTrigger value="skills">技能</TabsTrigger>
              <TabsTrigger value="channels">渠道</TabsTrigger>
              <TabsTrigger value="settings">设置</TabsTrigger>
            </TabsList>

            <TabsContent value="skills" className="flex-1 m-0 mt-0">
              <SkillStore onExecute={handleSkillExecute} />
            </TabsContent>

            <TabsContent value="channels" className="flex-1 m-0 mt-0">
              <ChannelConfig />
            </TabsContent>

            <TabsContent value="settings" className="flex-1 m-0 mt-0">
              <div className="p-4">
                <h3 className="font-semibold mb-4">设置</h3>
                <p className="text-sm text-muted-foreground">
                  设置功能待实现...
                </p>
              </div>
            </TabsContent>
          </Tabs>
        </SheetContent>
      </Sheet>
    </div>
  );
}
```

- [ ] **步骤 6：更新 App.tsx**

```tsx
// ui/clawteam-chat/src/App.tsx
import { ChatInterface } from "@/features/chat/ChatInterface";
import { Provider } from "react-redux";
import { store } from "@/store";

function App() {
  return (
    <Provider store={store}>
      <div className="flex h-screen bg-background text-foreground">
        <ChatInterface />
      </div>
    </Provider>
  );
}

export default App;
```

- [ ] **步骤 7：Commit**

```bash
git add ui/clawteam-chat/src/features/chat/ ui/clawteam-chat/src/App.tsx
git commit -m "feat(chat): enhance ChatInterface with AgentPanel, SkillStore, and ChannelConfig integration"
```

---

## 任务 8：最终集成与测试

**文件：**
- 修改：`ui/clawteam-chat/package.json` (添加 sheet 组件依赖)
- 运行构建测试

- [ ] **步骤 1：安装 Sheet 组件依赖**

```bash
cd ui/clawteam-chat && pnpm add @radix-ui/react-dialog
```

- [ ] **步骤 2：创建 sheet.tsx 组件**

```tsx
// ui/clawteam-chat/src/components/ui/sheet.tsx
import * as React from "react";
import * as DialogPrimitive from "@radix-ui/react-dialog";
import { X } from "lucide-react";
import { cn } from "@/lib/utils";

const Sheet = DialogPrimitive.Root;
const SheetTrigger = DialogPrimitive.Trigger;
const SheetClose = DialogPrimitive.Close;
const SheetPortal = DialogPrimitive.Portal;

const SheetOverlay = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Overlay>,
  React.ComponentPropsWithoutRef<typeof DialogPrimitive.Overlay>
>(({ className, ...props }, ref) => (
  <DialogPrimitive.Overlay
    className={cn(
      "fixed inset-0 z-50 bg-black/80 data-[state=open]:animate-in data-[state=closed]:animate-out data-[state=closed]:fade-out-0 data-[state=open]:fade-in-0",
      className
    )}
    {...props}
    ref={ref}
  />
));
SheetOverlay.displayName = DialogPrimitive.Overlay.displayName;

interface SheetContentProps
  extends React.ComponentPropsWithoutRef<typeof DialogPrimitive.Content> {
  side?: "top" | "bottom" | "left" | "right";
}

const sheetVariants = {
  top: "inset-x-0 top-0 border-b data-[state=closed]:slide-out-to-top data-[state=open]:slide-in-from-top",
  bottom:
    "inset-x-0 bottom-0 border-t data-[state=closed]:slide-out-to-bottom data-[state=open]:slide-in-from-bottom",
  left: "inset-y-0 left-0 h-full w-3/4 border-r data-[state=closed]:slide-out-to-left data-[state=open]:slide-in-from-left sm:max-w-sm",
  right:
    "inset-y-0 right-0 h-full w-3/4 border-l data-[state=closed]:slide-out-to-right data-[state=open]:slide-in-from-right sm:max-w-sm",
};

const SheetContent = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Content>,
  SheetContentProps
>(({ side = "right", className, children, ...props }, ref) => (
  <SheetPortal>
    <SheetOverlay />
    <DialogPrimitive.Content
      ref={ref}
      className={cn(
        "fixed z-50 gap-4 bg-background p-6 shadow-lg transition ease-in-out data-[state=open]:animate-in data-[state=closed]:animate-out data-[state=closed]:duration-300 data-[state=open]:duration-500",
        sheetVariants[side],
        className
      )}
      {...props}
    >
      {children}
      <DialogPrimitive.Close className="absolute right-4 top-4 rounded-sm opacity-70 ring-offset-background transition-opacity hover:opacity-100 focus:outline-none focus:ring-2 focus:ring-ring focus:ring-offset-2 disabled:pointer-events-none data-[state=open]:bg-secondary">
        <X className="h-4 w-4" />
        <span className="sr-only">Close</span>
      </DialogPrimitive.Close>
    </DialogPrimitive.Content>
  </SheetPortal>
));
SheetContent.displayName = DialogPrimitive.Content.displayName;

const SheetHeader = ({
  className,
  ...props
}: React.HTMLAttributes<HTMLDivElement>) => (
  <div
    className={cn(
      "flex flex-col space-y-2 text-center sm:text-left",
      className
    )}
    {...props}
  />
);
SheetHeader.displayName = "SheetHeader";

const SheetFooter = ({
  className,
  ...props
}: React.HTMLAttributes<HTMLDivElement>) => (
  <div
    className={cn(
      "flex flex-col-reverse sm:flex-row sm:justify-end sm:space-x-2",
      className
    )}
    {...props}
  />
);
SheetFooter.displayName = "SheetFooter";

const SheetTitle = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Title>,
  React.ComponentPropsWithoutRef<typeof DialogPrimitive.Title>
>(({ className, ...props }, ref) => (
  <DialogPrimitive.Title
    ref={ref}
    className={cn("text-lg font-semibold text-foreground", className)}
    {...props}
  />
));
SheetTitle.displayName = DialogPrimitive.Title.displayName;

const SheetDescription = React.forwardRef<
  React.ElementRef<typeof DialogPrimitive.Description>,
  React.ComponentPropsWithoutRef<typeof DialogPrimitive.Description>
>(({ className, ...props }, ref) => (
  <DialogPrimitive.Description
    ref={ref}
    className={cn("text-sm text-muted-foreground", className)}
    {...props}
  />
));
SheetDescription.displayName = DialogPrimitive.Description.displayName;

export {
  Sheet,
  SheetPortal,
  SheetOverlay,
  SheetTrigger,
  SheetClose,
  SheetContent,
  SheetHeader,
  SheetFooter,
  SheetTitle,
  SheetDescription,
};
```

- [ ] **步骤 3：运行类型检查**

```bash
cd ui/clawteam-chat && pnpm check
```

预期：无类型错误

- [ ] **步骤 4：运行构建**

```bash
cd ui/clawteam-chat && pnpm build
```

预期：构建成功

- [ ] **步骤 5：启动开发服务器测试**

```bash
cd ui/clawteam-chat && pnpm dev
```

预期：开发服务器启动成功，界面可正常访问

- [ ] **步骤 6：Final Commit**

```bash
git add ui/clawteam-chat/
git commit -m "feat(ui): complete ClawTeam frontend UI implementation with Agent panel, Skill store, and channel configuration"
```

---

## 验证清单

- [ ] Agent 面板正常显示，可添加/配置/启动 Agent
- [ ] Skill 商店正常显示，可选择和执行技能
- [ ] 飞书配置组件正常工作，可保存配置
- [ ] 微信配置组件正常工作，可保存配置
- [ ] 聊天界面正常显示消息
- [ ] 输入框支持发送消息
- [ ] 右侧面板 Sheet 正常打开/关闭
- [ ] 类型检查通过
- [ ] 构建成功
