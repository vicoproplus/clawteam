<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch } from 'vue'
import { Terminal } from '@xterm/xterm'
import { FitAddon } from '@xterm/addon-fit'
import { WebLinksAddon } from '@xterm/addon-web-links'
import '@xterm/xterm/css/xterm.css'
import { useTerminalStore } from '@/stores/terminal'
import { writeToTerminal, resizeTerminal } from '@/lib/terminal'
import type { TerminalSession } from '@/types/terminal'

interface Props {
  session: TerminalSession
  active: boolean
}

const props = defineProps<Props>()

const terminalRef = ref<HTMLElement | null>(null)
const terminalStore = useTerminalStore()

let terminal: Terminal | null = null
let fitAddon: FitAddon | null = null

onMounted(() => {
  if (!terminalRef.value) return

  terminal = new Terminal({
    fontSize: 13,
    fontFamily: 'Consolas, Monaco, monospace',
    theme: {
      background: '#1e1e1e',
      foreground: '#d4d4d4',
      cursor: '#ffffff',
      selectionBackground: 'rgba(255, 255, 255, 0.3)',
    },
    cursorBlink: true,
    scrollback: 5000,
  })

  fitAddon = new FitAddon()
  terminal.loadAddon(fitAddon)
  terminal.loadAddon(new WebLinksAddon())

  terminal.open(terminalRef.value)
  fitAddon.fit()

  // 处理输入
  terminal.onData(async (data) => {
    await writeToTerminal(props.session.id, data)
  })

  // 处理调整大小
  terminal.onResize(async ({ cols, rows }) => {
    await resizeTerminal(props.session.id, cols, rows)
  })

  // 初始输出
  const output = terminalStore.outputs[props.session.id]
  if (output) {
    terminal.write(output)
  }
})

watch(() => terminalStore.outputs[props.session.id], (newOutput) => {
  if (terminal && newOutput) {
    terminal.write(newOutput)
    terminalStore.outputs[props.session.id] = ''
  }
})

onUnmounted(() => {
  terminal?.dispose()
})
</script>

<template>
  <div 
    ref="terminalRef" 
    :class="[
      'h-full w-full bg-[#1e1e1e]',
      !active && 'opacity-50'
    ]"
  />
</template>
