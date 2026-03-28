import { createPinia } from 'pinia'

export const pinia = createPinia()

export const setupPinia = () => {
  // Pinia 插件可以在这里添加
  return pinia
}
