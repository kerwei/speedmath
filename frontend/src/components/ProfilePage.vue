<script setup>
// 个人资料页 (gèrén zīliào yè — profile page)
// 查看用户名/邮箱; 修改用户名 (180 天锁定) 与密码
import { ref, computed, onMounted } from 'vue'
import { useI18n } from '../composables/useI18n.js'
import { useAuth } from '../composables/useAuth.js'

const { t } = useI18n()
const { token, isLoggedIn } = useAuth()
const emit = defineEmits(['back', 'login'])

const profile = ref(null)
const loadError = ref('')

// 用户名表单 (yònghùmíng biǎodān — username form)
const username = ref('')
const unameMsg = ref('')
const unameErr = ref('')
const unameBusy = ref(false)

// 密码表单 (mìmǎ biǎodān — password form)
const curPw = ref('')
const newPw = ref('')
const pwMsg = ref('')
const pwErr = ref('')
const pwBusy = ref(false)

const LOCK_DAYS = 180
const DAY_MS = 24 * 60 * 60 * 1000

// 锁定状态 (suǒdìng zhuàngtài — lock state shown under the username field)
const lockInfo = computed(() => {
  if (!profile.value) return null
  const last = profile.value.username_updated_at
  if (!last) return { state: 'never', text: t('profile.never') }
  // 后端格式 "YYYY-MM-DD HH:MM:SS" (UTC) → 可解析的 ISO
  const lastDate = new Date(last.replace(' ', 'T') + 'Z')
  const next = new Date(lastDate.getTime() + LOCK_DAYS * DAY_MS)
  if (Date.now() < next.getTime()) {
    return { state: 'locked', text: t('profile.nextChange') + ' ' + next.toLocaleDateString() }
  }
  return { state: 'free', text: t('profile.canChange') }
})

async function apiGet(path) {
  const res = await fetch(path, { headers: { 'Authorization': 'Bearer ' + token.value } })
  return res.json()
}

async function apiPost(path, data) {
  const res = await fetch(path, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
      'Authorization': 'Bearer ' + token.value
    },
    body: new URLSearchParams(data)
  })
  return { status: res.status, data: await res.json() }
}

async function loadProfile() {
  loadError.value = ''
  const data = await apiGet('/api/user/profile')
  if (data.error) { loadError.value = data.error; return }
  profile.value = data
  username.value = data.username
}

async function saveUsername() {
  unameMsg.value = ''; unameErr.value = ''
  const name = username.value.trim()
  // 与后端规则一致的前端校验 (frontend mirror of backend rules)
  if (name.length < 3 || name.length > 20) { unameErr.value = t('profile.error.length'); return }
  if (!/^[\p{L}\p{Nd}]{3,20}$/u.test(name.normalize('NFC'))) { unameErr.value = t('profile.error.charset'); return }
  unameBusy.value = true
  const { status, data } = await apiPost('/api/user/username', { username: name })
  unameBusy.value = false
  if (data.error) {
    unameErr.value = status === 403 ? t('profile.locked')
      : status === 409 ? t('profile.error.taken')
      : data.error
    return
  }
  unameMsg.value = data.unchanged ? t('profile.unchanged') : t('profile.saved')
  profile.value.username = data.username
  username.value = data.username
}

async function savePassword() {
  pwMsg.value = ''; pwErr.value = ''
  if (!curPw.value || newPw.value.length < 4) { pwErr.value = t('profile.error.pwshort'); return }
  pwBusy.value = true
  const { status, data } = await apiPost('/api/user/password', {
    current_password: curPw.value,
    new_password: newPw.value
  })
  pwBusy.value = false
  if (data.error) {
    pwErr.value = status === 401 ? t('profile.error.pwbad') : data.error
    return
  }
  pwMsg.value = t('profile.pwsaved')
  curPw.value = ''; newPw.value = ''
}

onMounted(() => {
  if (isLoggedIn()) loadProfile()
})
</script>

<template>
  <div class="card">
    <div class="profile-header">
      <h2>{{ t('profile.title') }}</h2>
      <button class="btn btn-sm" @click="$emit('back')">← {{ t('profile.back') }}</button>
    </div>

    <!-- 未登录 (wèi dēnglù — not logged in): 登录入口 -->
    <div v-if="!isLoggedIn()" class="profile-login">
      <p class="profile-login-text">{{ t('profile.loginPrompt') }}</p>
      <button class="btn btn-primary" @click="$emit('login')" style="width:100%">
        {{ t('profile.login') }}
      </button>
    </div>

    <template v-else>
      <div v-if="loadError" class="profile-error">{{ loadError }}</div>
      <div v-else-if="!profile" class="profile-info">{{ t('profile.loading') }}</div>

      <template v-else>
        <div class="form-group">
          <label>{{ t('profile.email') }}</label>
          <div class="profile-static">{{ profile.email }}</div>
        </div>
        <div class="form-group">
          <label>{{ t('profile.registered') }}</label>
          <div class="profile-static">{{ (profile.created_at || '').slice(0, 10) }}</div>
        </div>

        <hr class="profile-divider" />

        <div class="form-group">
          <label>{{ t('profile.username') }}</label>
          <input class="answer-input" v-model="username" :placeholder="t('profile.username')"
            maxlength="20" @keydown="e => e.key === 'Enter' && saveUsername()" />
          <div class="profile-hint">{{ t('profile.username.hint') }}</div>
          <div class="profile-lock" :class="lockInfo?.state">{{ lockInfo?.text }}</div>
        </div>
        <div v-if="unameMsg" class="profile-msg">{{ unameMsg }}</div>
        <div v-if="unameErr" class="profile-error">{{ unameErr }}</div>
        <button class="btn btn-primary" @click="saveUsername" :disabled="unameBusy" style="width:100%">
          {{ unameBusy ? t('lobby.busy') : t('profile.save') }}
        </button>

        <hr class="profile-divider" />

        <div class="form-group">
          <label>{{ t('profile.password') }}</label>
        </div>
        <div class="form-group">
          <label>{{ t('profile.currentpw') }}</label>
          <input class="answer-input" v-model="curPw" type="password" :placeholder="t('profile.currentpw')" />
        </div>
        <div class="form-group">
          <label>{{ t('profile.newpw') }}</label>
          <input class="answer-input" v-model="newPw" type="password" :placeholder="t('profile.newpw')"
            @keydown="e => e.key === 'Enter' && savePassword()" />
        </div>
        <div v-if="pwMsg" class="profile-msg">{{ pwMsg }}</div>
        <div v-if="pwErr" class="profile-error">{{ pwErr }}</div>
        <button class="btn btn-secondary" @click="savePassword" :disabled="pwBusy" style="width:100%">
          {{ pwBusy ? t('lobby.busy') : t('profile.save') }}
        </button>
      </template>
    </template>
  </div>
</template>

<style scoped>
.profile-header { display: flex; justify-content: space-between; align-items: center; }
.profile-static { padding: 0.6rem; background: rgba(255,255,255,0.04); border-radius: 8px; color: #ddd; }
.profile-hint { font-size: 0.8rem; color: #888; margin-top: 0.3rem; }
.profile-msg { color: #4caf50; font-size: 0.85rem; margin-bottom: 0.5rem; }
.profile-error { color: #f44336; font-size: 0.85rem; margin-bottom: 0.5rem; }
.profile-lock { font-size: 0.8rem; margin-top: 0.3rem; }
.profile-lock.never { color: #b0b0ff; }
.profile-lock.free { color: #4caf50; }
.profile-lock.locked { color: #ff9800; }
.profile-divider { border: none; border-top: 1px solid rgba(255,255,255,0.08); margin: 1.2rem 0; }
.profile-login { text-align: center; padding: 1.5rem 0; }
.profile-login-text { color: #b0b0ff; font-size: 0.9rem; margin-bottom: 1rem; }
</style>
