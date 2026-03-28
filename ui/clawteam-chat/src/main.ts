import { createApp } from 'vue'
import { setupPinia } from '@/stores'
import { router } from '@/router'
import App from '@/App.vue'
import '@/index.css'

const app = createApp(App)

app.use(setupPinia())
app.use(router)

app.mount('#app')
