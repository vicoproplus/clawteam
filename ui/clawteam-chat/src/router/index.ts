import { createRouter, createWebHistory } from 'vue-router'
import type { RouteRecordRaw } from 'vue-router'
import ChatView from '@/views/ChatView.vue'
import Settings from '@/components/Settings.vue'

const routes: RouteRecordRaw[] = [
  {
    path: '/',
    name: 'chat',
    component: ChatView,
  },
  {
    path: '/session/:sessionId',
    name: 'session',
    component: ChatView,
  },
  {
    path: '/terminal',
    name: 'terminal',
    component: () => import('@/components/TerminalWorkspace.vue'),
  },
  {
    path: '/settings',
    name: 'settings',
    component: Settings,
  },
]

export const router = createRouter({
  history: createWebHistory(),
  routes,
})