<script setup lang="ts">
</script>

<template>
  <div>
    <h3 class="text-base font-bold text-gray-200 mb-4 pb-2.5 border-b border-gray-700">我的账号</h3>
    <div class="divide-y divide-gray-800/50">
      <div class="py-3 flex items-center justify-between">
        <div class="font-medium text-sm text-gray-200">名称</div>
        <div class="text-sm text-gray-500">群主</div>
      </div>
      <div class="py-3 flex items-center justify-between">
        <div class="font-medium text-sm text-gray-200">邮箱</div>
        <div class="text-sm text-gray-500">name@example.com</div>
      </div>
      <div class="py-3 flex items-center justify-between">
        <div class="font-medium text-sm text-gray-200">时区</div>
        <select class="bg-gray-800 border border-gray-700 rounded-lg px-2.5 py-1.5 text-sm text-gray-400 outline-none cursor-pointer">
          <option selected>UTC 协调世界时</option>
          <option>Asia/Shanghai (UTC+8)</option>
        </select>
      </div>
    </div>
  </div>
</template>
