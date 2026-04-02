<script setup lang="ts">
import { UserPlus, PenTool } from 'lucide-vue-next'
import { useAgentStore } from '@/stores/agent'
import ExpertPanel from '@/components/agent/ExpertPanel.vue'
import type { AgentConfig, AgentRole } from '@/types/agent'

const agentStore = useAgentStore()

const roleColors: Record<AgentRole, string> = {
  assistant: 'bg-blue-500',
  worker: 'bg-emerald-500',
  supervisor: 'bg-orange-500',
}

function getAgentsByRole(role: AgentRole): AgentConfig[] {
  return agentStore.agentList
    .map(id => agentStore.agents[id]?.config)
    .filter((a): a is AgentConfig => a?.role === role)
}
</script>

<template>
  <div class="h-full flex flex-col overflow-hidden">
    <!-- Top Bar -->
    <div class="px-7 py-4 flex justify-between items-center border-b border-gray-700 bg-gray-800">
      <h2 class="text-xl font-bold text-white">员工管理</h2>
      <div class="flex items-center gap-4">
        <div class="flex items-center gap-1.5 text-sm text-gray-500">
          排序
          <span class="bg-gray-800 border border-gray-700 px-2.5 py-0.5 rounded-md text-xs text-gray-300 cursor-pointer hover:bg-gray-700">
            默认
          </span>
        </div>
        <button class="flex items-center gap-1.5 px-3 py-2 rounded-lg text-sm text-gray-400 border border-gray-700 hover:bg-gray-700 hover:text-gray-200 transition-colors">
          <UserPlus class="w-4 h-4" />
          添加员工
        </button>
      </div>
    </div>

    <!-- Main Content -->
    <div class="flex-1 overflow-y-auto">
      <div class="grid grid-cols-3 gap-5 px-7 py-6 max-lg:grid-cols-1">
        <!-- Assistants Column -->
        <div>
          <div class="flex items-center justify-between mb-3">
            <div class="flex items-center gap-2 text-sm font-semibold text-gray-500">
              <span class="w-2 h-2 rounded-full bg-blue-500"></span>
              助手
            </div>
            <span class="text-xs text-gray-600">{{ getAgentsByRole('assistant').length }}</span>
          </div>

          <div v-if="getAgentsByRole('assistant').length === 0" class="py-8 border border-dashed border-gray-700 rounded-xl flex flex-col items-center justify-center gap-2 min-h-[120px]">
            <span class="text-gray-600 text-2xl">😐</span>
            <p class="text-xs text-gray-600">暂无助手</p>
          </div>

          <div v-else class="space-y-3">
            <div v-for="agent in getAgentsByRole('assistant')" :key="agent.id"
              class="p-3.5 rounded-xl bg-gray-800 border border-gray-700 hover:border-gray-600 transition-colors group">
              <div class="flex items-center justify-between">
                <div class="flex items-center gap-3">
                  <div :class="['w-10 h-10 rounded-full flex items-center justify-center text-white font-bold text-sm relative', roleColors[agent.role]]">
                    {{ agent.name.charAt(0) }}
                    <div class="absolute -bottom-0.5 -right-0.5 w-2.5 h-2.5 bg-green-500 border-2 border-gray-800 rounded-full"></div>
                  </div>
                  <div>
                    <div class="text-sm font-semibold text-gray-200">{{ agent.name }}</div>
                    <div class="text-xs text-gray-500">已激活</div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>

        <!-- Supervisors Column -->
        <div>
          <div class="flex items-center justify-between mb-3">
            <div class="flex items-center gap-2 text-sm font-semibold text-gray-500">
              <span class="w-2 h-2 rounded-full bg-orange-500"></span>
              监工
            </div>
            <span class="text-xs text-gray-600">{{ getAgentsByRole('supervisor').length }}</span>
          </div>

          <div v-if="getAgentsByRole('supervisor').length === 0" class="py-8 border border-dashed border-gray-700 rounded-xl flex flex-col items-center justify-center gap-2 min-h-[120px]">
            <span class="text-gray-600 text-2xl">🛡️</span>
            <p class="text-xs text-gray-600">暂无监工</p>
          </div>

          <div v-else class="space-y-3">
            <div v-for="agent in getAgentsByRole('supervisor')" :key="agent.id"
              class="p-3.5 rounded-xl bg-gray-800 border border-gray-700 hover:border-gray-600 transition-colors group">
              <div class="flex items-center justify-between">
                <div class="flex items-center gap-3">
                  <div :class="['w-10 h-10 rounded-full flex items-center justify-center text-white font-bold text-sm', roleColors[agent.role]]">
                    {{ agent.name.charAt(0) }}
                  </div>
                  <div>
                    <div class="text-sm font-semibold text-gray-200">{{ agent.name }}</div>
                    <div class="text-xs text-gray-500">已激活</div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>

        <!-- Custom Experts Column -->
        <div>
          <div class="flex items-center justify-between mb-3">
            <div class="flex items-center gap-2 text-sm font-semibold text-gray-500">
              <span class="w-2 h-2 rounded-full bg-purple-500"></span>
              自定义专家
            </div>
          </div>

          <div class="p-5 rounded-xl border border-dashed border-gray-700 flex flex-col items-center justify-center gap-2.5 cursor-pointer hover:border-purple-500 hover:bg-purple-500/10 transition-colors min-h-[140px]">
            <div class="w-11 h-11 rounded-full bg-gray-800 flex items-center justify-center text-gray-500">
              <PenTool class="w-5 h-5" />
            </div>
            <span class="text-xs text-gray-500">写入提示词创建专家</span>
          </div>
        </div>
      </div>

      <!-- Expert Panel -->
      <ExpertPanel />
    </div>
  </div>
</template>
