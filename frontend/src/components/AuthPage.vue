<script setup>
import { ref } from 'vue'
import { useI18n } from '../composables/useI18n.js'
import { useAuth } from '../composables/useAuth.js'

const { t } = useI18n()
const { register, login } = useAuth()
const emit = defineEmits(['done'])

const mode = ref('login')  // 'login' | 'register'
const email = ref('')
const password = ref('')
const error = ref('')
const busy = ref(false)

async function submit() {
  error.value = ''
  if (!email.value.trim() || !password.value) {
    error.value = t('auth.error.required')
    return
  }
  if (mode.value === 'register' && password.value.length < 4) {
    error.value = t('auth.error.short')
    return
  }
  busy.value = true
  const fn = mode.value === 'register' ? register : login
  const data = await fn(email.value.trim(), password.value)
  busy.value = false
  if (data.error) {
    error.value = data.error === 'email taken' ? t('auth.error.taken')
      : data.error === 'invalid credentials' ? t('auth.error.badlogin')
      : t('auth.error.unknown')
    return
  }
  emit('done')
}

function toggleMode() {
  mode.value = mode.value === 'login' ? 'register' : 'login'
  error.value = ''
}
</script>

<template>
  <div class="card">
    <h2>{{ mode === 'login' ? t('auth.login') : t('auth.register') }}</h2>

    <div class="form-group">
      <label>{{ t('auth.email') }}</label>
      <input class="answer-input" v-model="email" type="email" :placeholder="t('auth.email')"
        @keydown="e => e.key === 'Enter' && submit()" />
    </div>

    <div class="form-group">
      <label>{{ t('auth.password') }}</label>
      <input class="answer-input" v-model="password" type="password" :placeholder="t('auth.password')"
        @keydown="e => e.key === 'Enter' && submit()" />
    </div>

    <div v-if="error" class="auth-error">{{ error }}</div>

    <button class="btn btn-primary" @click="submit" :disabled="busy" style="width:100%">
      {{ busy ? t('auth.busy') : (mode === 'login' ? t('auth.login') : t('auth.register')) }}
    </button>

    <p class="auth-toggle">
      {{ mode === 'login' ? t('auth.noaccount') : t('auth.hasaccount') }}
      <a href="#" @click.prevent="toggleMode">{{ mode === 'login' ? t('auth.register') : t('auth.login') }}</a>
    </p>
  </div>
</template>

<style scoped>
.form-group { margin-bottom: 1rem; text-align: left; }
.form-group label { display: block; margin-bottom: 0.3rem; color: #b0b0ff; font-size: 0.85rem; }
.auth-error { color: #f44336; font-size: 0.85rem; margin-bottom: 0.8rem; }
.auth-toggle { margin-top: 1rem; font-size: 0.85rem; color: #888; }
.auth-toggle a { color: #6c63ff; cursor: pointer; }
</style>
