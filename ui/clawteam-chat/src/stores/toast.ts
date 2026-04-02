// ui/clawteam-chat/src/stores/toast.ts
import { defineStore } from 'pinia'
import { ref } from 'vue'

export type ToastType = 'success' | 'error' | 'info'

export interface Toast {
  id: number
  type: ToastType
  message: string
}

export const useToastStore = defineStore('toast', () => {
  const toasts = ref<Toast[]>([])
  let nextId = 1

  function show(type: ToastType, message: string, duration = 2500) {
    const id = nextId++
    toasts.value.push({ id, type, message })
    setTimeout(() => remove(id), duration)
  }

  function remove(id: number) {
    toasts.value = toasts.value.filter((t) => t.id !== id)
  }

  function success(message: string) {
    show('success', message)
  }
  function error(message: string) {
    show('error', message)
  }
  function info(message: string) {
    show('info', message)
  }

  return { toasts, show, remove, success, error, info }
})
