import { createPinia } from 'pinia'

export const pinia = createPinia()

export const setupPinia = () => {
  // Pinia 插件可以在这里添加
  return pinia
}

// 导出 stores
export { useWorkspaceStore } from './workspace'
export { useProjectStore } from './project'
export { useChatStore } from './chat'
export { useAgentStore } from './agent'
export { useChannelStore } from './channel'
export { useSkillStore } from './skill'
export { useAuditStore } from './audit'
