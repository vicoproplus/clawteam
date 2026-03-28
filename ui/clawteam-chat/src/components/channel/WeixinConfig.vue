<script setup lang="ts">
import { storeToRefs } from 'pinia'
import { useChannelStore } from '@/stores/channel'
import { Input, Switch, Button } from '@/components/ui'
import { Save } from 'lucide-vue-next'

const channelStore = useChannelStore()
const { weixin, statuses } = storeToRefs(channelStore)

const handleSave = () => {
  channelStore.saveConfigs()
}
</script>

<template>
  <div class="space-y-4">
    <div class="flex items-center justify-between">
      <h3 class="font-medium">微信渠道配置</h3>
      <div class="flex items-center gap-2">
        <span
          :class="statuses.weixin.connected ? 'text-green-500' : 'text-gray-400'"
          class="text-xs"
        >
          {{ statuses.weixin.connected ? '已连接' : '未连接' }}
        </span>
        <Switch :model-value="weixin.enabled" @update:model-value="channelStore.updateWeixinConfig({ enabled: $event })" />
      </div>
    </div>

    <div class="space-y-3">
      <div>
        <label class="block text-sm text-gray-600 mb-1">App ID</label>
        <Input :model-value="weixin.appId" placeholder="wxaaaaaaaaxxxxxxxx" @update:model-value="channelStore.updateWeixinConfig({ appId: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">App Secret</label>
        <Input :model-value="weixin.appSecret" type="password" placeholder="微信应用密钥" @update:model-value="channelStore.updateWeixinConfig({ appSecret: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">Token</label>
        <Input :model-value="weixin.token" placeholder="消息校验 Token" @update:model-value="channelStore.updateWeixinConfig({ token: $event })" />
      </div>
      <div>
        <label class="block text-sm text-gray-600 mb-1">EncodingAESKey</label>
        <Input :model-value="weixin.encodingAESKey" placeholder="消息加解密密钥" @update:model-value="channelStore.updateWeixinConfig({ encodingAESKey: $event })" />
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
