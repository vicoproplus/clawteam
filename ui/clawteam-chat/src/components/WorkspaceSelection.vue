<script setup lang="ts">
import { ref } from 'vue'
import { useWorkspaceStore } from '@/stores/workspace'
import { useProjectStore } from '@/stores/project'
import { cn } from '@/lib/utils'
import { ChevronDown, Plus, FolderOpen } from 'lucide-vue-next'

const workspaceStore = useWorkspaceStore()
const projectStore = useProjectStore()

const isOpen = ref(false)

const handleSelectWorkspace = async (id: string) => {
  workspaceStore.selectWorkspace(id)
  await projectStore.loadProject(id)
  isOpen.value = false
}

const handleAddWorkspace = () => {
  // TODO: 打开文件夹选择对话框
  console.log('Add workspace')
}
</script>

<template>
  <div class="relative">
    <button
      :class="cn(
        'w-10 h-10 rounded-lg flex items-center justify-center transition-colors',
        isOpen ? 'bg-gray-700 text-white' : 'text-gray-400 hover:bg-gray-800 hover:text-gray-200'
      )"
      title="选择工作区"
      @click="isOpen = !isOpen"
    >
      <ChevronDown class="h-5 w-5" />
    </button>

    <!-- Dropdown -->
    <div
      v-if="isOpen"
      class="absolute left-12 top-0 w-56 bg-white rounded-lg shadow-lg border z-50"
    >
      <div class="p-2">
        <div class="text-xs text-gray-500 px-2 py-1">工作区</div>
        <button
          v-for="workspace in workspaceStore.workspaces"
          :key="workspace.id"
          :class="cn(
            'w-full px-2 py-2 text-left text-sm rounded-md flex items-center gap-2',
            workspace.id === workspaceStore.currentWorkspaceId
              ? 'bg-blue-50 text-blue-600'
              : 'hover:bg-gray-50'
          )"
          @click="handleSelectWorkspace(workspace.id)"
        >
          <FolderOpen class="h-4 w-4 text-gray-400" />
          <span class="truncate">{{ workspace.name }}</span>
        </button>
        
        <div class="border-t mt-2 pt-2">
          <button
            class="w-full px-2 py-2 text-left text-sm rounded-md flex items-center gap-2 hover:bg-gray-50 text-gray-600"
            @click="handleAddWorkspace"
          >
            <Plus class="h-4 w-4" />
            添加工作区
          </button>
        </div>
      </div>
    </div>
  </div>
</template>
