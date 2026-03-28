export type ChannelType = 'feishu' | 'weixin' | 'web'

export interface FeishuConfig {
  appId: string
  appSecret: string
  encryptKey: string
  verificationToken: string
  enabled: boolean
}

export interface WeixinConfig {
  appId: string
  appSecret: string
  token: string
  encodingAESKey: string
  enabled: boolean
}

export interface ChannelConfigs {
  feishu: FeishuConfig
  weixin: WeixinConfig
  web: { enabled: boolean }
}

export interface ChannelStatus {
  type: ChannelType
  connected: boolean
  lastConnectedAt?: number
  error?: string
}
