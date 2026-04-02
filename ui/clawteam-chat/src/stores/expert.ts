// ui/clawteam-chat/src/stores/expert.ts
import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import type { Expert, ExpertCategory } from '@/types/expert'

// 预置专家数据
const DEFAULT_EXPERTS: Expert[] = [
  {
    id: 1,
    name: 'Kai',
    role: '内容创作专家',
    category: '内容创作',
    desc: '擅长创作引人入胜的多平台内容，让品牌故事触达目标受众',
    tags: ['文案', '品牌', '社媒'],
    isAdded: true,
  },
  {
    id: 2,
    name: 'Phoebe',
    role: '数据分析报告师',
    category: '数据分析',
    desc: '将复杂数据转化为清晰可执行的业务报告，让数据驱动决策',
    tags: ['数据清洗', '可视化', '报告'],
    isAdded: true,
  },
  {
    id: 3,
    name: 'Jude',
    role: '中国电商运营专家',
    category: '市场营销',
    desc: '精通天猫京东拼多多等多平台运营，从选品到爆款一站式操盘',
    tags: ['电商', '运营', '选品'],
    isAdded: true,
  },
  {
    id: 4,
    name: 'Maya',
    role: '抖音策略师',
    category: '市场营销',
    desc: '精通抖音算法和内容生态，打造短视频爆款并实现商业化变现',
    tags: ['抖音', '短视频', '变现'],
    isAdded: false,
  },
  {
    id: 5,
    name: 'Sam',
    role: 'UI设计师',
    category: '设计',
    desc: '专注于用户体验与界面美学，打造直观且美观的数字产品界面',
    tags: ['UI', 'UX', '设计系统'],
    isAdded: false,
  },
  {
    id: 6,
    name: 'Ula',
    role: '销售教练',
    category: '产品管理',
    desc: '提供实战销售技巧培训，帮助团队提升转化率与客单价',
    tags: ['销售', '培训', '转化'],
    isAdded: false,
  },
  {
    id: 7,
    name: 'Ben',
    role: '品牌策略师',
    category: '市场营销',
    desc: '构建品牌核心价值，制定长期品牌发展战略与视觉识别系统',
    tags: ['品牌', '策略', 'VI'],
    isAdded: false,
  },
  {
    id: 8,
    name: 'Fay',
    role: '小红书运营专家',
    category: '内容创作',
    desc: '深谙小红书种草逻辑，通过笔记优化与社群互动提升品牌曝光',
    tags: ['小红书', '种草', '社群'],
    isAdded: false,
  },
]

export const useExpertStore = defineStore('expert', () => {
  const experts = ref<Expert[]>([...DEFAULT_EXPERTS])
  const activeCategory = ref<ExpertCategory>('全部')
  const searchQuery = ref('')
  let nextId = 100

  // Getters
  const categories = computed(() => {
    const cats = new Set(experts.value.map((e) => e.category))
    return ['全部', ...Array.from(cats)] as ExpertCategory[]
  })

  const filteredExperts = computed(() => {
    return experts.value.filter((e) => {
      const matchCategory =
        activeCategory.value === '全部' || e.category === activeCategory.value
      const q = searchQuery.value.toLowerCase()
      const matchSearch =
        !q ||
        e.name.toLowerCase().includes(q) ||
        e.role.toLowerCase().includes(q) ||
        e.desc.toLowerCase().includes(q)
      return matchCategory && matchSearch
    })
  })

  const addedExperts = computed(() => experts.value.filter((e) => e.isAdded))

  const categoryCounts = computed(() => {
    const counts: Record<string, number> = { 全部: experts.value.length }
    for (const e of experts.value) {
      counts[e.category] = (counts[e.category] || 0) + 1
    }
    return counts
  })

  // Actions
  function setActiveCategory(category: ExpertCategory) {
    activeCategory.value = category
  }

  function setSearchQuery(query: string) {
    searchQuery.value = query
  }

  function addExpertToProject(id: number) {
    const expert = experts.value.find((e) => e.id === id)
    if (expert) {
      expert.isAdded = true
    }
  }

  function createExpert(expert: Omit<Expert, 'id' | 'isAdded'>) {
    experts.value.unshift({
      ...expert,
      id: nextId++,
      isAdded: true,
    })
  }

  function removeExpert(id: number) {
    const expert = experts.value.find((e) => e.id === id)
    if (expert) {
      expert.isAdded = false
    }
  }

  return {
    experts,
    activeCategory,
    searchQuery,
    categories,
    filteredExperts,
    addedExperts,
    categoryCounts,
    setActiveCategory,
    setSearchQuery,
    addExpertToProject,
    createExpert,
    removeExpert,
  }
})
