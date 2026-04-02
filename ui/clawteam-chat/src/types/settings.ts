// ui/clawteam-chat/src/types/settings.ts

export type Theme = 'dark' | 'light' | 'system'

export interface CliConfig {
  id: string
  name: string
  command: string
  installed: boolean
  custom?: boolean
  icon?: string
}

export interface ShellConfig {
  id: string
  name: string
  path: string
}

export interface SettingsState {
  theme: Theme
  selectedCli: string
  selectedShell: string
  cliTools: CliConfig[]
  shells: ShellConfig[]
  pathPool: string[]
}
