<script setup lang="ts">
import { Plus, CircleCheck, FolderOpen, RefreshCw, List } from 'lucide-vue-next'
import { useSettingsStore } from '@/stores/settings'
import { useToastStore } from '@/stores/toast'
import type { CliConfig } from '@/types/settings'

const settingsStore = useSettingsStore()
const toastStore = useToastStore()

function handleSelectCli(id: string) {
  settingsStore.selectCli(id)
  const tool = settingsStore.cliTools.find(t => t.id === id)
  toastStore.success(`已选择「${tool?.name || id}」作为默认CLI`)
}

function getIconClass(tool: CliConfig) {
  if (settingsStore.selectedCli === tool.id) return 'bg-sky-500/20 text-sky-400 border-sky-500/40'
  if (tool.installed) return 'bg-amber-500/15 text-amber-400 border-amber-500/30'
  return 'bg-gray-800 text-gray-500'
}
</script>

<template>
  <div>
    <div class="mb-8">
      <h3 class="text-base font-bold text-gray-200 mb-2">CLI配置</h3>
      <p class="text-xs text-gray-500">配置终端调用CLI方式，选择默认的AI助手工具</p>
    </div>

    <!-- CLI Grid -->
    <div class="grid grid-cols-4 gap-3 mb-6 max-xl:grid-cols-3 max-lg:grid-cols-2 max-sm:grid-cols-1">
      <div v-for="tool in settingsStore.cliTools" :key="tool.id" @click="handleSelectCli(tool.id)"
        :class="['bg-gray-800 border rounded-xl p-4 flex flex-col items-center text-center gap-1.5 cursor-pointer relative transition-all hover:bg-gray-700 hover:border-gray-600',
          settingsStore.selectedCli === tool.id ? 'border-sky-500 bg-sky-500/10' : 'border-gray-700', tool.custom ? 'border-dashed' : '']">
        <CircleCheck v-if="settingsStore.selectedCli === tool.id" class="absolute top-2 right-2 w-3 h-3 text-sky-400" />
        <div :class="['w-12 h-12 rounded-xl flex items-center justify-center text-lg border', getIconClass(tool)]">⚡</div>
        <div class="font-semibold text-sm text-gray-200">{{ tool.name }}</div>
        <div class="text-xs text-gray-500 font-mono max-w-full truncate">{{ tool.command }}</div>
        <div class="text-xs text-gray-500">{{ tool.custom ? '自定义终端' : '默认终端' }}</div>
        <div v-if="!tool.custom" :class="['text-xs', tool.installed ? 'text-amber-400' : 'text-red-400']">
          {{ tool.installed ? '已安装' : '未安装' }}
        </div>
      </div>

      <!-- Add CLI Card -->
      <div class="border border-dashed border-gray-700 rounded-xl p-4 flex flex-col items-center justify-center gap-2 cursor-pointer hover:border-sky-500 hover:bg-sky-500/10 transition-colors min-h-[130px]">
        <div class="w-12 h-12 rounded-xl bg-gray-800 flex items-center justify-center">
          <Plus class="w-5 h-5 text-gray-500" />
        </div>
        <span class="text-sm text-gray-500">自定义 CLI</span>
      </div>
    </div>

    <div class="h-px bg-gray-700 my-6"></div>

    <!-- Shell Selection -->
    <div class="mb-8">
      <div class="flex justify-between items-center mb-3">
        <span class="text-sm text-gray-500">选择终端</span>
        <div class="flex items-center gap-4">
          <div class="flex items-center gap-1.5 text-xs text-gray-500">
            编码
            <select class="bg-gray-800 border border-gray-700 rounded-md px-2 py-1 text-xs text-gray-400 outline-none">
              <option>自动 (推荐)</option>
              <option>UTF-8</option>
              <option>GBK</option>
            </select>
          </div>
          <button class="text-xs text-sky-400 flex items-center gap-1 hover:text-sky-300">
            <RefreshCw class="w-3 h-3" />
            刷新列表
          </button>
        </div>
      </div>

      <div class="grid grid-cols-2 gap-3 max-sm:grid-cols-1">
        <div v-for="shell in settingsStore.shells" :key="shell.id" @click="settingsStore.selectShell(shell.id)"
          :class="['bg-gray-800 border rounded-lg px-4 py-3.5 flex justify-between items-center cursor-pointer transition-all hover:bg-gray-700 hover:border-gray-600 relative',
            settingsStore.selectedShell === shell.id ? 'border-sky-500 bg-sky-500/10' : 'border-gray-700']">
          <CircleCheck v-if="settingsStore.selectedShell === shell.id" class="absolute right-8 text-sky-400 w-3 h-3" />
          <div>
            <div class="font-semibold text-sm text-gray-200 mb-0.5">{{ shell.name }}</div>
            <div class="text-xs text-gray-500 font-mono max-w-[220px] truncate">{{ shell.path }}</div>
          </div>
        </div>

        <!-- Add Shell Card -->
        <div class="border border-dashed border-gray-700 rounded-lg py-5 flex flex-col items-center justify-center gap-2 cursor-pointer hover:border-sky-500 hover:bg-sky-500/10 transition-colors">
          <div class="w-9 h-9 rounded-lg bg-gray-800 flex items-center justify-center">
            <Plus class="w-4 h-4 text-gray-500" />
          </div>
          <span class="text-sm text-gray-500">自定义终端</span>
        </div>
      </div>
    </div>

    <!-- PATH Pool -->
    <div>
      <h3 class="text-base font-bold text-gray-200 mb-2">兜底 PATH 路径池</h3>
      <p class="text-xs text-gray-500 mb-4">仅在命令检测失败或启动终端成员时，用于兜底查找与 PATH 注入。</p>

      <div class="flex gap-2 mb-3">
        <input type="text" placeholder="每行输入一条路径 (目录或可执行文件路径)"
          class="flex-1 bg-gray-800 border border-gray-700 rounded-lg px-3.5 py-2.5 text-sm text-gray-200 outline-none focus:border-sky-500 focus:ring-2 focus:ring-sky-500/20" />
        <button class="px-4 py-2.5 rounded-lg text-sm text-gray-400 border border-gray-700 hover:bg-gray-700 hover:text-gray-200 transition-colors flex items-center gap-1.5">
          <FolderOpen class="w-4 h-4" />
          选择文件夹
        </button>
        <button class="px-4 py-2.5 rounded-lg text-sm bg-sky-500/20 border border-sky-500/40 text-sky-400 hover:bg-sky-500/30 transition-colors flex items-center gap-1.5">
          <Plus class="w-4 h-4" />
          添加
        </button>
      </div>

      <div class="flex justify-between items-center pt-2.5 border-t border-gray-800">
        <span class="text-xs text-gray-500">{{ settingsStore.pathPool.length }} 条路径</span>
        <button class="text-xs text-gray-500 flex items-center gap-1 hover:text-gray-300">
          <List class="w-3 h-3" />
          管理列表
        </button>
      </div>
    </div>
  </div>
</template>
