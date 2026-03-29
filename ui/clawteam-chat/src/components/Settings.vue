<script setup lang="ts">
import { ref } from 'vue'
import ChannelConfig from './channel/ChannelConfig.vue'
import { Button, Switch } from '@/components/ui'
import { Save } from 'lucide-vue-next'

const channelConfigOpen = ref(false)

// 通用设置
const settings = ref({
  theme: 'system',
  language: 'zh-CN',
  autoStartAgents: true,
  soundEnabled: true,
})

const handleSaveSettings = () => {
  console.log('Save settings:', settings.value)
}
</script>

<template>
  <div class="h-full flex">
    <!-- Settings Sidebar -->
    <div class="w-48 border-r bg-gray-50 p-3">
      <nav class="space-y-1">
        <button class="w-full px-3 py-2 text-sm text-left rounded-md bg-white shadow-sm">
          通用设置
        </button>
        <button 
          class="w-full px-3 py-2 text-sm text-left rounded-md hover:bg-gray-100"
          @click="channelConfigOpen = true"
        >
          渠道配置
        </button>
        <button class="w-full px-3 py-2 text-sm text-left rounded-md hover:bg-gray-100">
          关于
        </button>
      </nav>
    </div>

    <!-- Settings Content -->
    <div class="flex-1 overflow-auto p-6">
      <div class="max-w-2xl">
        <h1 class="text-xl font-semibold mb-6">通用设置</h1>
        
        <div class="space-y-6">
          <!-- 外观 -->
          <div>
            <h2 class="text-sm font-medium text-gray-500 uppercase tracking-wide mb-3">外观</h2>
            <div class="space-y-4">
              <div class="flex items-center justify-between">
                <div>
                  <div class="font-medium text-sm">主题</div>
                  <div class="text-xs text-gray-500">选择界面主题</div>
                </div>
                <select v-model="settings.theme" class="border rounded-md px-3 py-1.5 text-sm">
                  <option value="system">跟随系统</option>
                  <option value="light">浅色</option>
                  <option value="dark">深色</option>
                </select>
              </div>
              
              <div class="flex items-center justify-between">
                <div>
                  <div class="font-medium text-sm">语言</div>
                  <div class="text-xs text-gray-500">界面语言</div>
                </div>
                <select v-model="settings.language" class="border rounded-md px-3 py-1.5 text-sm">
                  <option value="zh-CN">简体中文</option>
                  <option value="en-US">English</option>
                </select>
              </div>
            </div>
          </div>

          <!-- 行为 -->
          <div>
            <h2 class="text-sm font-medium text-gray-500 uppercase tracking-wide mb-3">行为</h2>
            <div class="space-y-4">
              <div class="flex items-center justify-between">
                <div>
                  <div class="font-medium text-sm">自动启动 Agent</div>
                  <div class="text-xs text-gray-500">工作区打开时自动启动配置的 Agent</div>
                </div>
                <Switch v-model="settings.autoStartAgents" />
              </div>
              
              <div class="flex items-center justify-between">
                <div>
                  <div class="font-medium text-sm">声音提醒</div>
                  <div class="text-xs text-gray-500">接收消息时播放提示音</div>
                </div>
                <Switch v-model="settings.soundEnabled" />
              </div>
            </div>
          </div>
        </div>

        <div class="mt-8 pt-4 border-t">
          <Button @click="handleSaveSettings">
            <Save class="h-4 w-4 mr-2" />
            保存设置
          </Button>
        </div>
      </div>
    </div>

    <!-- Channel Config Modal -->
    <ChannelConfig
      :open="channelConfigOpen"
      @update:open="channelConfigOpen = $event"
    />
  </div>
</template>
