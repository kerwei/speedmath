import { createApp } from 'vue'
import App from './App.vue'
import './style.css'

// 移动端检测 (yídòng duān jiǎncè — mobile detection): 触摸设备才加载移动端样式表 (chùmō shèbèi cái jiāzài yídòng duān yàngshì biǎo — only load the mobile stylesheet on touch devices)
function isMobileDevice() {
  if (typeof window.matchMedia === 'function' && window.matchMedia('(pointer: coarse)').matches) return true
  return /Android|iPhone|iPad|iPod|Mobile/i.test(navigator.userAgent || '')
}

if (isMobileDevice()) {
  import('./mobile.css')
}

createApp(App).mount('#app')
