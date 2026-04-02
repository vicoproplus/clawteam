<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import AccountSettings from '@/components/settings/AccountSettings.vue'
import AppearanceSettings from '@/components/settings/AppearanceSettings.vue'
import CliSettings from '@/components/settings/CliSettings.vue'

const route = useRoute()

const activeTab = computed(() => {
  const parts = route.path.split('/')
  return parts[2] || 'account'
})
</script>

<template>
  <div class="h-full flex">
    <div class="flex-1 overflow-y-auto p-8">
      <AccountSettings v-if="activeTab === 'account'" />
      <AppearanceSettings v-else-if="activeTab === 'appearance'" />
      <CliSettings v-else-if="activeTab === 'cli'" />
    </div>
  </div>
</template>
