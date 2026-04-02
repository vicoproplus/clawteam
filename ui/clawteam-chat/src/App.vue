<script setup lang="ts">
import { onMounted } from 'vue'
import { RouterView } from 'vue-router'
import { useWorkspaceStore } from '@/stores/workspace'
import { useProjectStore } from '@/stores/project'
import { useAgentStore } from '@/stores/agent'
import { useChannelStore } from '@/stores/channel'
import { useSkillStore } from '@/stores/skill'

const workspaceStore = useWorkspaceStore()
const projectStore = useProjectStore()
const agentStore = useAgentStore()
const channelStore = useChannelStore()
const skillStore = useSkillStore()

onMounted(async () => {
  await Promise.all([
    workspaceStore.loadWorkspaces(),
    agentStore.loadAgents(),
    channelStore.loadConfigs(),
    skillStore.loadSkills(),
  ])
  if (workspaceStore.currentWorkspaceId) {
    await projectStore.loadProject(workspaceStore.currentWorkspaceId)
  }
})
</script>

<template>
  <RouterView />
</template>
