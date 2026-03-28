<script setup lang="ts">
import { ref } from 'vue'
import { Dialog } from '@/components/ui'
import FeishuConfig from './FeishuConfig.vue'
import WeixinConfig from './WeixinConfig.vue'
import { MessageSquare, MessageCircle } from 'lucide-vue-next'

interface Props {
  open: boolean
}

defineProps<Props>()
const emit = defineEmits<{
  'update:open': [value: boolean]
}>()

const activeTab = ref<'feishu' | 'weixin'>('feishu')
</script>

<template>
  <Dialog :open="open" class="max-w-lg" @update:open="emit('update:open', $event)">
    <div class="space-y-4">
      <h2 class="text-lg font-semibold">渠道配置</h2>

      <div class="flex border-b">
        <button
          :class="activeTab === 'feishu' ? 'border-blue-500 text-blue-600' : 'border-transparent text-gray-500 hover:text-gray-700'"
          class="px-4 py-2 text-sm font-medium border-b-2 transition-colors"
          @click="activeTab = 'feishu'"
        >
          <MessageSquare class="h-4 w-4 inline mr-1.5" />
          飞书
        </button>
        <button
          :class="activeTab === 'weixin' ? 'border-blue-500 text-blue-600' : 'border-transparent text-gray-500 hover:text-gray-700'"
          class="px-4 py-2 text-sm font-medium border-b-2 transition-colors"
          @click="activeTab = 'weixin'"
        >
          <MessageCircle class="h-4 w-4 inline mr-1.5" />
          微信
        </button>
      </div>

      <FeishuConfig v-if="activeTab === 'feishu'" />
      <WeixinConfig v-if="activeTab === 'weixin'" />
    </div>
  </Dialog>
</template>
