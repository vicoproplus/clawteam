<!-- ui/clawteam-chat/src/components/common/Toast.vue -->
<script setup lang="ts">
import { storeToRefs } from 'pinia'
import { useToastStore, type ToastType } from '@/stores/toast'
import { CheckCircle, XCircle, Info } from 'lucide-vue-next'

const toastStore = useToastStore()
const { toasts } = storeToRefs(toastStore)

const iconMap: Record<ToastType, typeof CheckCircle> = {
  success: CheckCircle,
  error: XCircle,
  info: Info,
}

const colorMap: Record<ToastType, string> = {
  success: 'text-green-500',
  error: 'text-red-500',
  info: 'text-sky-500',
}
</script>

<template>
  <Teleport to="body">
    <div class="fixed top-5 right-5 z-[1000] flex flex-col gap-2">
      <TransitionGroup name="toast">
        <div
          v-for="toast in toasts"
          :key="toast.id"
          class="bg-gray-800 border border-gray-700 rounded-xl px-4 py-3 text-sm text-gray-200 shadow-lg flex items-center gap-2.5 min-w-[260px]"
        >
          <component :is="iconMap[toast.type]" :class="['w-4 h-4', colorMap[toast.type]]" />
          <span>{{ toast.message }}</span>
        </div>
      </TransitionGroup>
    </div>
  </Teleport>
</template>

<style scoped>
.toast-enter-active {
  animation: toastIn 0.3s ease;
}
.toast-leave-active {
  animation: toastOut 0.3s ease;
}

@keyframes toastIn {
  from {
    opacity: 0;
    transform: translateX(30px);
  }
  to {
    opacity: 1;
    transform: translateX(0);
  }
}

@keyframes toastOut {
  from {
    opacity: 1;
    transform: translateX(0);
  }
  to {
    opacity: 0;
    transform: translateX(30px);
  }
}
</style>
