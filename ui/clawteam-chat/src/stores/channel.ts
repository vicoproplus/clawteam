import { ref } from 'vue'
import { defineStore, acceptHMRUpdate } from 'pinia'
import type { FeishuConfig, WeixinConfig, ChannelStatus, ChannelType } from '@/types/channel'

export const useChannelStore = defineStore('channel', () => {
  const feishu = ref<FeishuConfig>({
    appId: '',
    appSecret: '',
    encryptKey: '',
    verificationToken: '',
    enabled: false,
  })

  const weixin = ref<WeixinConfig>({
    appId: '',
    appSecret: '',
    token: '',
    encodingAESKey: '',
    enabled: false,
  })

  const statuses = ref<Record<ChannelType, ChannelStatus>>({
    feishu: { type: 'feishu', connected: false },
    weixin: { type: 'weixin', connected: false },
    web: { type: 'web', connected: true },
  })

  const updateFeishuConfig = (updates: Partial<FeishuConfig>) => {
    feishu.value = { ...feishu.value, ...updates }
  }

  const updateWeixinConfig = (updates: Partial<WeixinConfig>) => {
    weixin.value = { ...weixin.value, ...updates }
  }

  const updateChannelStatus = (type: ChannelType, status: Partial<ChannelStatus>) => {
    statuses.value[type] = { ...statuses.value[type], ...status }
  }

  const loadConfigs = async () => {
    try {
      const response = await fetch('/api/channels')
      if (!response.ok) return
      const data = await response.json()
      if (data.feishu) feishu.value = { ...feishu.value, ...data.feishu }
      if (data.weixin) weixin.value = { ...weixin.value, ...data.weixin }
    } catch {
      // 使用默认值
    }
  }

  const saveConfigs = async () => {
    try {
      await fetch('/api/channels', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ feishu: feishu.value, weixin: weixin.value }),
      })
    } catch (e) {
      console.error('Failed to save channel configs:', e)
    }
  }

  return {
    feishu,
    weixin,
    statuses,
    updateFeishuConfig,
    updateWeixinConfig,
    updateChannelStatus,
    loadConfigs,
    saveConfigs,
  }
})

if (import.meta.hot) {
  import.meta.hot.accept(acceptHMRUpdate(useChannelStore, import.meta.hot))
}
