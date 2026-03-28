<script setup lang="ts">
import { ref } from 'vue'
import { storeToRefs } from 'pinia'
import { useAgentStore } from '@/stores/agent'
import AgentCard from './AgentCard.vue'
import AgentConfigModal from './agent/AgentConfigModal.vue'
import { Button } from './ui'
import { Plus } from 'lucide-vue-next'
import type { AgentRole, AgentConfig } from '@/types/agent'

const agentStore = useAgentStore()
const { selectedAgentId, groupedByRole } = storeToRefs(agentStore)

const roleOrder: AgentRole[] = ['assistant', 'worker', 'supervisor']
const roleLabels: Record<AgentRole, string> = {
  assistant: '助手',
  worker: '员工',
  supervisor: '监工',
}

const configModalOpen = ref(false)
const editingAgent = ref<AgentConfig | null>(null)

const handleAddAgent = (_role: AgentRole) => {
  editingAgent.value = null
  configModalOpen.value = true
}

const handleEditAgent = (id: string) => {
  const agent = agentStore.agents[id]
  if (agent) {
    editingAgent.value = { ...agent.config }
    configModalOpen.value = true
  }
}

const handleSaveConfig = (config: AgentConfig) => {
  agentStore.updateAgent(config.id, config)
  configModalOpen.value = false
  editingAgent.value = null
}

const handleCardAction = (action: string, id: string) => {
  if (action === 'config') handleEditAgent(id)
  else if (action === 'start') console.log('Start:', id)
  else if (action === 'stop') console.log('Stop:', id)
}
</script>

<template>
  <div class="w-64 border-r bg-gray-50 flex flex-col">
    <div class="p-4 border-b flex items-center justify-between">
      <h2 class="font-semibold text-sm">Agent 成员</h2>
    </div>

    <div class="flex-1 overflow-auto p-3 space-y-4">
      <template v-for="role in roleOrder" :key="role">
        <div v-if="groupedByRole[role]?.length">
          <div class="text-xs font-medium text-gray-500 mb-2 uppercase tracking-wide">
            {{ roleLabels[role] }}
          </div>
          <div class="space-y-1.5">
            <AgentCard
              v-for="agent in groupedByRole[role]"
              :key="agent.config.id"
              :id="agent.config.id"
              :name="agent.config.name"
              :role="agent.config.role"
              :status="agent.status"
              :cli-tool="agent.config.cliTool"
              :required="agent.config.role === 'assistant'"
              :selected="selectedAgentId === agent.config.id"
              @click="agentStore.selectAgent(agent.config.id)"
              @action="handleCardAction($event, agent.config.id)"
            />
          </div>
        </div>
      </template>
    </div>

    <div class="p-3 border-t space-y-1.5">
      <Button variant="outline" size="sm" class="w-full text-xs" @click="handleAddAgent('worker')">
        <Plus class="h-3 w-3 mr-1" />
        添加 Agent
      </Button>
    </div>

    <AgentConfigModal
      :open="configModalOpen"
      :agent="editingAgent"
      @update:open="configModalOpen = $event"
      @save="handleSaveConfig"
    />
  </div>
</template>
