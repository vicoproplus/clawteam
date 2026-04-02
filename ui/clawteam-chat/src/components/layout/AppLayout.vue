<script setup lang="ts">
import GlobalNav from './GlobalNav.vue'
import ContextSidebar from './ContextSidebar.vue'
import Toast from '@/components/common/Toast.vue'
</script>

<template>
  <div class="h-screen flex bg-[#0B0F19] text-gray-200">
    <GlobalNav />
    <ContextSidebar />
    <main class="flex-1 overflow-hidden flex flex-col">
      <slot />
    </main>
    <Toast />
  </div>
</template>
