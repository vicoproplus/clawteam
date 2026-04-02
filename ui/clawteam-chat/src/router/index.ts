import { createRouter, createWebHistory } from 'vue-router'
import type { RouteRecordRaw } from 'vue-router'
import AppLayout from '@/components/layout/AppLayout.vue'
import ChatView from '@/views/ChatView.vue'
import AgentsView from '@/views/AgentsView.vue'
import SkillsView from '@/views/SkillsView.vue'
import SettingsView from '@/views/SettingsView.vue'

const routes: RouteRecordRaw[] = [
  {
    path: '/',
    component: AppLayout,
    children: [
      { path: '', redirect: '/chat' },
      { path: 'chat', name: 'chat', component: ChatView },
      { path: 'session/:sessionId', name: 'session', component: ChatView },
      { path: 'agents', name: 'agents', component: AgentsView },
      { path: 'skills', name: 'skills', component: SkillsView },
      {
        path: 'settings',
        name: 'settings',
        component: SettingsView,
        children: [
          { path: '', redirect: 'account' },
          { path: 'account', name: 'settings-account', component: SettingsView },
          { path: 'appearance', name: 'settings-appearance', component: SettingsView },
          { path: 'cli', name: 'settings-cli', component: SettingsView },
        ],
      },
      {
        path: 'terminal',
        name: 'terminal',
        component: () => import('@/components/TerminalWorkspace.vue'),
      },
    ],
  },
]

export const router = createRouter({
  history: createWebHistory(),
  routes,
})
