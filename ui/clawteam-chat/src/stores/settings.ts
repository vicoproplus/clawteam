// ui/clawteam-chat/src/stores/settings.ts
import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { Theme, CliConfig, ShellConfig } from '@/types/settings'

const DEFAULT_CLI_TOOLS: CliConfig[] = [
  { id: 'gemini', name: 'Gemini CLI', command: 'gemini', installed: false },
  { id: 'codex', name: 'Codex', command: 'codex', installed: false },
  { id: 'claude', name: 'Claude Code', command: 'claude', installed: true },
  { id: 'opencode', name: 'opencode', command: 'opencode', installed: false },
  { id: 'qwen', name: 'Qwen Code', command: 'qwen', installed: false },
  { id: 'openclaw', name: 'OpenClaw', command: 'openclaw', installed: false },
  { id: 'iflow', name: 'iflow', command: 'iflow -p', installed: true, custom: true },
]

const DEFAULT_SHELLS: ShellConfig[] = [
  { id: 'default', name: '系统默认', path: '使用系统默认 shell' },
  { id: 'powershell', name: 'Windows PowerShell', path: 'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe' },
  { id: 'cmd', name: 'Command Prompt', path: 'C:\\Windows\\system32\\cmd.exe' },
]

export const useSettingsStore = defineStore('settings', () => {
  const theme = ref<Theme>('dark')
  const selectedCli = ref<string>('iflow')
  const selectedShell = ref<string>('cmd')
  const cliTools = ref<CliConfig[]>([...DEFAULT_CLI_TOOLS])
  const shells = ref<ShellConfig[]>([...DEFAULT_SHELLS])
  const pathPool = ref<string[]>([])

  function setTheme(newTheme: Theme) {
    theme.value = newTheme
    if (newTheme === 'dark') {
      document.documentElement.classList.add('dark')
    } else if (newTheme === 'light') {
      document.documentElement.classList.remove('dark')
    }
  }

  function selectCli(id: string) {
    selectedCli.value = id
  }

  function selectShell(id: string) {
    selectedShell.value = id
  }

  function addCliTool(tool: Omit<CliConfig, 'id'>) {
    const id = `custom-${Date.now()}`
    cliTools.value.push({ ...tool, id, custom: true })
  }

  function removeCliTool(id: string) {
    cliTools.value = cliTools.value.filter((t) => t.id !== id)
    if (selectedCli.value === id) {
      selectedCli.value = cliTools.value[0]?.id || ''
    }
  }

  function addPath(path: string) {
    if (path && !pathPool.value.includes(path)) {
      pathPool.value.push(path)
    }
  }

  function removePath(path: string) {
    pathPool.value = pathPool.value.filter((p) => p !== path)
  }

  return {
    theme,
    selectedCli,
    selectedShell,
    cliTools,
    shells,
    pathPool,
    setTheme,
    selectCli,
    selectShell,
    addCliTool,
    removeCliTool,
    addPath,
    removePath,
  }
})
