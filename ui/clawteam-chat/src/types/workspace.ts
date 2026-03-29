// src/types/workspace.ts
import type { TerminalType, TerminalConnectionStatus } from './terminal';

export interface WorkspaceEntry {
  id: string;
  name: string;
  path: string;
  lastOpenedAt: number;
}

export interface Member {
  id: string;
  name: string;
  role: string;
  roleKey: string;
  roleType: 'owner' | 'admin' | 'assistant' | 'member';
  avatar: string;
  status: MemberStatus;
  manualStatus?: MemberStatus;
  terminalStatus?: TerminalConnectionStatus;
  terminalType?: TerminalType;
  terminalCommand?: string;
  terminalPath?: string;
  autoStartTerminal: boolean;
}

export type MemberStatus = 'online' | 'working' | 'dnd' | 'offline';

export interface ProjectData {
  projectId: string;
  version: number;
  members: Member[];
  memberSequence: Record<string, number>;
  terminal: {
    recentClosedTabs: ProjectTerminalRecentTab[];
  };
  roadmap: {
    objective: string;
    tasks: RoadmapTask[];
  };
  skills: {
    current: ProjectSkill[];
  };
}

export interface ProjectTerminalRecentTab {
  id: string;
  memberId: string;
  closedAt: number;
}

export interface RoadmapTask {
  id: string;
  title: string;
  status: 'pending' | 'in-progress' | 'completed';
}

export interface ProjectSkill {
  id: string;
  name: string;
  enabled: boolean;
}
