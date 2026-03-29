<script setup lang="ts">
import { onMounted } from 'vue'
import { RouterView } from 'vue-router'
import SidebarNav from '@/components/SidebarNav.vue'
import { useWorkspaceStore } from '@/stores/workspace'
import { useProjectStore } from '@/stores/project'
import { useAgentStore } from '@/stores/agent'
import { useChannelStore } from '@/stores/channel'

const workspaceStore = useWorkspaceStore()
const projectStore = useProjectStore()
const agentStore = useAgentStore()
const channelStore = useChannelStore()

onMounted(async () => {
  await workspaceStore.loadWorkspaces()
  if (workspaceStore.currentWorkspaceId) {
    await projectStore.loadProject(workspaceStore.currentWorkspaceId)
  }
  await agentStore.loadAgents()
  await channelStore.loadConfigs()
})
</script>

<template>
  <div class="h-screen flex bg-gray-50">
    <SidebarNav />
    <main class="flex-1 overflow-hidden">
      <RouterView />
    </main>
  </div>
</template>