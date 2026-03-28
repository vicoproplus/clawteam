<script setup lang="ts">
import { storeToRefs } from 'pinia'
import { useAgentStore } from '@/stores/agent'
import AgentCard from './AgentCard.vue'
import { Button } from './ui'
import { Plus } from 'lucide-vue-next'
import type { AgentRole } from '@/types/agent'

const agentStore = useAgentStore()
const { selectedAgentId, groupedByRole } = storeToRefs(agentStore)

const roleOrder: AgentRole[] = ['assistant', 'worker', 'supervisor']
const roleLabels: Record<AgentRole, string> = {
  assistant: '助手',
  worker: '员工',
  supervisor: '监工',
}
</script>

<template>
  <div class="w-64 border-r bg-gray-50 flex flex-col">
    <div class="p-4 border-b">
      <h2 class="font-semibold">Agent 成员</h2>
    </div>
    
    <div class="flex-1 overflow-auto p-4 space-y-4">
      <template v-for="role in roleOrder" :key="role">
        <div v-if="groupedByRole[role]?.length">
          <div class="text-xs font-medium text-gray-500 mb-2">
            {{ roleLabels[role] }}
          </div>
          <div class="space-y-2">
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
            />
          </div>
        </div>
      </template>
    </div>
    
    <div class="p-4 border-t">
      <Button variant="outline" size="sm" class="w-full">
        <Plus class="h-4 w-4 mr-2" />
        添加 Agent
      </Button>
    </div>
  </div>
</template>
