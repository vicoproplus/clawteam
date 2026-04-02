// ui/clawteam-chat/src/types/expert.ts

export interface Expert {
  id: number
  name: string
  role: string
  category: string
  desc: string
  tags: string[]
  added: boolean
  avatar?: string
}

export const EXPERT_CATEGORIES = [
  '全部',
  '设计',
  '工程技术',
  '内容创作',
  '数据分析',
  '产品管理',
  '市场营销',
  '其他',
] as const

export type ExpertCategory = (typeof EXPERT_CATEGORIES)[number]
