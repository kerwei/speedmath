// 认证状态管理 (rènzhèng zhuàngtài guǎnlǐ — auth state management)
import { ref } from 'vue'

const token = ref(localStorage.getItem('auth_token') || '')
const userId = ref(parseInt(localStorage.getItem('auth_user_id') || '0'))

export function useAuth() {
  function isLoggedIn() { return !!token.value }

  async function apiAuth(endpoint, email, password) {
    const params = new URLSearchParams({ email, password })
    const res = await fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: params
    })
    const data = await res.json()
    if (data.token) {
      token.value = data.token
      userId.value = data.user_id || 0
      localStorage.setItem('auth_token', data.token)
      localStorage.setItem('auth_user_id', String(data.user_id || 0))
    }
    return data
  }

  async function register(email, password) {
    return apiAuth('/api/auth/register', email, password)
  }

  async function login(email, password) {
    return apiAuth('/api/auth/login', email, password)
  }

  function logout() {
    token.value = ''
    userId.value = 0
    localStorage.removeItem('auth_token')
    localStorage.removeItem('auth_user_id')
  }

  return { token, userId, isLoggedIn, register, login, logout }
}
