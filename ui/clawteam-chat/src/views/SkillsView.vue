<script setup lang="ts">
import { ref, computed } from 'vue'
import { Search, ChevronLeft, ChevronRight } from 'lucide-vue-next'
import { useSkillStore } from '@/stores/skill'
import { useToastStore } from '@/stores/toast'
import SkillCard from '@/components/skill/SkillCard.vue'
import type { Skill } from '@/types/skill'

const skillStore = useSkillStore()
const toastStore = useToastStore()

const activeTab = ref('store')
const activeFilter = ref('all')
const searchQuery = ref('')

const tabs = [
  { id: 'store', label: '商店' },
  { id: 'mine', label: '我的技能' },
  { id: 'project', label: '项目技能' },
]

const filters = [
  { id: 'all', label: '全部技能' },
  { id: 'quality', label: '代码质量' },
  { id: 'docs', label: '文档处理' },
]

const filteredSkills = computed(() => {
  let skills = skillStore.skillsArray
  if (searchQuery.value) {
    const q = searchQuery.value.toLowerCase()
    skills = skills.filter(s => s.name.toLowerCase().includes(q) || s.description.toLowerCase().includes(q))
  }
  return skills
})

function handleInstall(skill: Skill) {
  toastStore.success(`「${skill.name}」安装成功`)
}
</script>

<template>
  <div class="h-full overflow-y-auto p-6">
    <div class="flex items-center gap-3 mb-2">
      <h2 class="text-lg font-bold text-white">技能</h2>
      <p class="text-xs text-gray-500">浏览、安装和管理项目技能</p>
    </div>

    <!-- Search and Tabs -->
    <div class="flex items-center gap-3 mb-4 flex-wrap">
      <div class="relative max-w-[360px]">
        <Search class="absolute left-3.5 top-1/2 -translate-y-1/2 w-4 h-4 text-gray-500" />
        <input v-model="searchQuery" type="text" placeholder="搜索技能..."
          class="w-full bg-gray-800 border border-gray-700 rounded-xl pl-10 pr-4 py-2.5 text-sm text-gray-200 outline-none focus:border-sky-500 focus:ring-2 focus:ring-sky-500/20" />
      </div>
      <div class="flex gap-0.5 bg-gray-800 rounded-lg p-0.5 border border-gray-700">
        <button v-for="tab in tabs" :key="tab.id" @click="activeTab = tab.id"
          :class="['px-4 py-1.5 text-sm rounded-md transition-colors',
            activeTab === tab.id ? 'bg-gray-700 text-gray-200 font-medium shadow' : 'text-gray-500 hover:text-gray-300']">
          {{ tab.label }}
        </button>
      </div>
    </div>

    <!-- Filters -->
    <div class="flex gap-1.5 mb-5 flex-wrap">
      <button v-for="filter in filters" :key="filter.id" @click="activeFilter = filter.id"
        :class="['px-3.5 py-1.5 rounded-full text-xs border transition-colors',
          activeFilter === filter.id ? 'bg-sky-500/20 border-sky-500 text-sky-400' : 'border-gray-700 text-gray-500 hover:border-gray-600 hover:text-gray-300']">
        {{ filter.label }}
      </button>
    </div>

    <!-- Skill Grid -->
    <div class="space-y-3">
      <SkillCard v-for="skill in filteredSkills" :key="skill.id" :skill="skill" @install="handleInstall" />
    </div>

    <!-- Pagination -->
    <div class="flex items-center justify-center gap-1 mt-6 pt-4 border-t border-gray-700">
      <button class="w-8 h-8 rounded-md border border-gray-700 flex items-center justify-center text-gray-500 hover:bg-gray-700 hover:text-gray-200 transition-colors" disabled>
        <ChevronLeft class="w-3 h-3" />
      </button>
      <button class="w-8 h-8 rounded-md border border-sky-500 bg-sky-500 text-gray-900 font-semibold text-sm">1</button>
      <button class="w-8 h-8 rounded-md border border-gray-700 flex items-center justify-center text-gray-500 hover:bg-gray-700 hover:text-gray-200 transition-colors">2</button>
      <button class="w-8 h-8 rounded-md border border-gray-700 flex items-center justify-center text-gray-500 hover:bg-gray-700 hover:text-gray-200 transition-colors">
        <ChevronRight class="w-3 h-3" />
      </button>
    </div>
  </div>
</template>
