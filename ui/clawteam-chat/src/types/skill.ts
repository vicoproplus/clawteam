// Skill 变量类型
export type SkillVarType = "text" | "number" | "file" | "directory" | "select";

export interface SkillVariable {
  name: string;
  label: string;
  varType: SkillVarType;
  required: boolean;
  default?: string;
  options?: string[]; // for select type
}

// Skill 执行模板
export interface SkillTemplate {
  messageTemplate: string;
  variables: SkillVariable[];
  timeoutMs?: number;
}

// Skill 定义
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
  applicableRoles: string[];
  template: SkillTemplate;
}
