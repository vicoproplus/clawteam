import { createRouter, createWebHistory } from 'vue-router'
import type { RouteRecordRaw } from 'vue-router'

const routes: RouteRecordRaw[] = [
  {
    path: '/',
    name: 'home',
    component: () => import('@/views/ChatView.vue'),
  },
  {
    path: '/session/:sessionId',
    name: 'session',
    component: () => import('@/views/ChatView.vue'),
  },
]

export const router = createRouter({
  history: createWebHistory(),
  routes,
})
