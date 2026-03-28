<script setup lang="ts">
import { storeToRefs } from 'pinia'
import { useChannelStore } from '@/stores/channel'
import { Input, Switch, Button } from '@/components/ui'
import { Save } from 'lucide-vue-next'

const channelStore = useChannelStore()
const { feishu, statuses } = storeToRefs(channelStore)

const handleSave = () => {
  channelStore.saveConfigs()
}
</script>

<template>
  <div class="space-y-4">
    <div class="flex items-center justify-between">
      <h3 class="font-medium">飞书渠道配置</h3>
      <div class="flex items-center gap-2">
        <span
          :class="statuses.feishu.connected ? 'text-green-500' : 'text-gray-400'"
          class="text-xs"
        >
          {{ statuses.feishu.connected ? '已连接' : '未连接' }}
        </span>
        <Switch :model-value="feishu.enabled" @update:model-value="channelStore.updateFeishuConfig({ enabled: $event })" />
      </div>
    </div>

    <div class="space-y-3">
      <div>
        <label class="block text-sm text-gray-600 mb-1">App ID</label>
        <Input :model-value="feishu.appId" placeholder="cli_xxxxxxxx" @update:model-value="channelStore.updateFeishuConfig({ appId: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">App Secret</label>
        <Input :model-value="feishu.appSecret" type="password" placeholder="飞书应用密钥" @update:model-value="channelStore.updateFeishuConfig({ appSecret: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">Encrypt Key</label>
        <Input :model-value="feishu.encryptKey" placeholder="可选" @update:model-value="channelStore.updateFeishuConfig({ encryptKey: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">Verification Token</label>
        <Input :model-value="feishu.verificationToken" placeholder="可选" @update:model-value="channelStore.updateFeishuConfig({ verificationToken: $event })" />
      </div>
    </div>

    <div class="pt-3 border-t">
      <Button size="sm" @click="handleSave">
        <Save class="h-3.5 w-3.5 mr-1.5" />
        保存配置
      </Button>
    </div>
  </div>
</template>
