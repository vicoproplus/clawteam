<script setup lang="ts">
import { Search } from 'lucide-vue-next'
import { useExpertStore } from '@/stores/expert'
import { useToastStore } from '@/stores/toast'
import ExpertCard from './ExpertCard.vue'
import type { Expert, ExpertCategory } from '@/types/expert'

const expertStore = useExpertStore()
const toastStore = useToastStore()

function handleAddExpert(expert: Expert) {
  if (expert.isAdded) {
    toastStore.info(`「${expert.name}」已在项目中`)
    return
  }
  expertStore.addExpertToProject(expert.id)
  toastStore.success(`已将「${expert.name}」添加到项目`)
}

function handleTabClick(category: ExpertCategory) {
  expertStore.setActiveCategory(category)
}
</script>

<template>
  <div class="mx-7 mb-6 rounded-xl border border-gray-600 overflow-hidden flex flex-col min-h-[420px]">
    <!-- Header -->
    <div class="px-6 py-5 border-b border-gray-700 bg-gray-800/60">
      <div class="flex justify-between items-start mb-4">
        <div>
          <h2 class="text-lg font-bold text-white mb-1">专家中心</h2>
          <p class="text-xs text-gray-500">按行业分类浏览专家，召唤他们为你服务</p>
        </div>
        <div class="relative w-64">
          <Search class="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-gray-500" />
          <input
            :value="expertStore.searchQuery"
            @input="expertStore.setSearchQuery(($event.target as HTMLInputElement).value)"
            type="text"
            placeholder="搜索专家职称或描述..."
            class="w-full bg-gray-800 border border-gray-700 rounded-lg pl-9 pr-3 py-2 text-sm text-gray-200 outline-none focus:border-purple-500 focus:ring-2 focus:ring-purple-500/20"
          />
        </div>
      </div>

      <!-- Category Tabs -->
      <div class="flex gap-6 overflow-x-auto border-b border-gray-700 pb-0 -mb-px">
        <button v-for="cat in expertStore.categories" :key="cat" @click="handleTabClick(cat)"
          :class="['text-sm py-2 border-b-2 transition-colors whitespace-nowrap bg-transparent border-t-0 border-l-0 border-r-0',
            expertStore.activeCategory === cat ? 'text-purple-400 border-purple-500 font-medium' : 'text-gray-500 border-transparent hover:text-gray-400']">
          {{ cat }}
          <span v-if="cat !== '全部'" class="text-xs ml-1 text-gray-600">({{ expertStore.categoryCounts[cat] || 0 }})</span>
        </button>
      </div>
    </div>

    <!-- Expert Grid -->
    <div class="flex-1 overflow-y-auto p-5 bg-gray-900/40">
      <div v-if="expertStore.filteredExperts.length === 0" class="text-center py-12 text-gray-500">
        <p>没有找到匹配的专家</p>
      </div>
      <div v-else class="grid grid-cols-4 gap-4 max-lg:grid-cols-3 max-md:grid-cols-2">
        <ExpertCard v-for="(expert, i) in expertStore.filteredExperts" :key="expert.id" :expert="expert"
          @add="handleAddExpert" class="animate-fade-in" :style="{ animationDelay: `${i * 40}ms` }" />
      </div>
    </div>
  </div>
</template>

<style scoped>
@keyframes fade-in {
  from { opacity: 0; transform: translateY(6px); }
  to { opacity: 1; transform: translateY(0); }
}
.animate-fade-in { animation: fade-in 0.3s ease both; }
</style>
