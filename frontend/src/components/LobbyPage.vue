<script setup>
import { ref, onUnmounted } from 'vue'
import { useI18n } from '../composables/useI18n.js'
import { useAuth } from '../composables/useAuth.js'

const { t } = useI18n()
const { token, userId, logout } = useAuth()
const emit = defineEmits(['start', 'back'])

const activeTab = ref('create')  // 'create' | 'join'
const roomCode = ref('')
const roomId = ref(0)
const roomStatus = ref(null)
const players = ref([])
const error = ref('')
const busy = ref(false)
let pollTimer = null

async function apiPost(path, data) {
  const params = new URLSearchParams(data)
  const res = await fetch(path, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
      'Authorization': 'Bearer ' + token.value
    },
    body: params
  })
  return res.json()
}

async function createRoom() {
  error.value = ''; busy.value = true
  const data = await apiPost('/api/room/create', {})
  busy.value = false
  if (data.error) { error.value = data.error; return }
  roomId.value = data.room_id
  roomCode.value = data.code
  roomStatus.value = 'waiting'
  startPolling()
}

async function joinRoom() {
  if (!roomCode.value.trim()) { error.value = t('lobby.error.nocode'); return }
  error.value = ''; busy.value = true
  const data = await apiPost('/api/room/join', { code: roomCode.value.trim().toUpperCase() })
  busy.value = false
  if (data.error) {
    error.value = data.error === 'room not found' ? t('lobby.error.notfound')
      : data.error === 'room not joinable' ? t('lobby.error.notjoinable')
      : data.error
    return
  }
  roomId.value = data.room_id
  roomCode.value = data.code
  roomStatus.value = 'waiting'
  startPolling()
}

async function leaveRoom() {
  await apiPost('/api/room/leave', { room_id: roomId.value })
  stopPolling()
  roomId.value = 0; roomCode.value = ''; roomStatus.value = null; players.value = []
}

async function toggleReady() {
  const me = players.value.find(p => p.me)
  if (!me) return
  await apiPost('/api/room/ready', { room_id: roomId.value, ready: !me.ready })
}

async function pollStatus() {
  if (!roomId.value) return
  const data = await fetch('/api/room/status?room_id=' + roomId.value, {
    headers: { 'Authorization': 'Bearer ' + token.value }
  }).then(r => r.json())
  if (data.error) return
  roomStatus.value = data.status
    players.value = (data.players || []).map(p => ({
      userId: p.user_id,
      username: p.username.split('@')[0],
      ready: p.ready,
      me: p.user_id === userId.value,
      isHost: p.user_id === data.host_id
    }))
}

function startPolling() {
  stopPolling()
  pollTimer = setInterval(pollStatus, 2000)
  pollStatus()
}

function stopPolling() {
  if (pollTimer) { clearInterval(pollTimer); pollTimer = null }
}

onUnmounted(() => stopPolling())

// Expose for cleanup
function handleBack() {
  if (roomId.value) leaveRoom()
  stopPolling()
  emit('back')
}

function handleLogout() {
  if (roomId.value) leaveRoom()
  stopPolling()
  logout()
  emit('back')
}
</script>

<template>
  <div class="card">
    <div class="lobby-header">
      <h2>{{ t('lobby.title') }}</h2>
      <div class="lobby-actions">
        <button class="btn btn-sm" @click="handleLogout">{{ t('auth.logout') }}</button>
      </div>
    </div>

    <!-- No room: show create / join tabs -->
    <div v-if="!roomCode">
      <div class="tab-bar">
        <button :class="['tab', { active: activeTab === 'create' }]" @click="activeTab = 'create'">
          {{ t('lobby.create') }}
        </button>
        <button :class="['tab', { active: activeTab === 'join' }]" @click="activeTab = 'join'">
          {{ t('lobby.join') }}
        </button>
        <button class="tab tab-back" @click="handleBack">← {{ t('lobby.back') }}</button>
      </div>

      <div v-if="activeTab === 'create'" class="tab-content">
        <p class="lobby-desc">{{ t('lobby.create.desc') }}</p>
        <button class="btn btn-primary" @click="createRoom" :disabled="busy" style="width:100%">
          {{ busy ? t('lobby.busy') : t('lobby.create.btn') }}
        </button>
      </div>

      <div v-else class="tab-content">
        <p class="lobby-desc">{{ t('lobby.join.desc') }}</p>
        <input class="answer-input" v-model="roomCode" :placeholder="t('lobby.code')"
          maxlength="4" style="text-transform:uppercase;text-align:center;font-size:1.5rem;letter-spacing:0.5em"
          @keydown="e => e.key === 'Enter' && joinRoom()" />
        <button class="btn btn-primary" @click="joinRoom" :disabled="busy || !roomCode.trim()" style="width:100%;margin-top:0.5rem">
          {{ busy ? t('lobby.busy') : t('lobby.join.btn') }}
        </button>
      </div>

      <div v-if="error" class="lobby-error">{{ error }}</div>

      <button class="btn btn-sm" @click="handleBack" style="margin-top:1rem">
        ← {{ t('lobby.back') }}
      </button>
    </div>

    <!-- In room: show player list -->
    <div v-else>
      <div class="room-info">
        <div class="room-code-label">{{ t('lobby.code') }}</div>
        <div class="room-code-value">{{ roomCode }}</div>
        <div class="room-share">{{ t('lobby.share') }}</div>
      </div>

      <div class="players-list">
        <div v-for="p in players" :key="p.userId" class="player-row"
          :class="{ 'player-me': p.me, 'player-host': p.isHost }">
          <span class="player-name">{{ p.username }}</span>
          <span class="player-tag" v-if="p.isHost">👑</span>
          <span class="player-status">{{ p.ready ? '✅' : '⏳' }}</span>
        </div>
      </div>

      <div class="lobby-actions">
        <button class="btn btn-primary" @click="toggleReady" style="flex:1">
          {{ players.find(p => p.me)?.ready ? t('lobby.unready') : t('lobby.ready') }}
        </button>
        <button class="btn btn-sm" @click="leaveRoom">{{ t('lobby.leave') }}</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.lobby-header { display: flex; justify-content: space-between; align-items: center; }
.lobby-actions { display: flex; gap: 0.5rem; }
.lobby-desc { font-size: 0.9rem; color: #b0b0ff; margin-bottom: 1rem; }
.lobby-error { color: #f44336; font-size: 0.85rem; margin-top: 0.5rem; }

.tab-bar { display: flex; gap: 0; margin-bottom: 1.5rem; border-radius: 8px; overflow: hidden; }
.tab { flex: 1; padding: 0.6rem; background: rgba(255,255,255,0.06); border: none; color: #888; cursor: pointer; font-size: 0.9rem; transition: all 0.2s; }
.tab.active { background: rgba(108,99,255,0.2); color: #b0b0ff; font-weight: 600; }
.tab-back { flex: 0; padding: 0.6rem 0.8rem; font-size: 0.8rem; }
.tab-content { margin-bottom: 0.5rem; }

.room-info { text-align: center; margin-bottom: 1.5rem; }
.room-code-label { font-size: 0.8rem; color: #888; margin-bottom: 0.3rem; }
.room-code-value { font-size: 3rem; font-weight: 700; letter-spacing: 0.3em; color: #ffd700; font-family: 'Courier New', monospace; }
.room-share { font-size: 0.8rem; color: #888; margin-top: 0.3rem; }

.players-list { margin-bottom: 1rem; }
.player-row { display: flex; align-items: center; gap: 0.5rem; padding: 0.6rem; border-radius: 8px; background: rgba(255,255,255,0.04); margin-bottom: 0.3rem; }
.player-me { background: rgba(255,215,0,0.08); border: 1px solid rgba(255,215,0,0.15); }
.player-name { flex: 1; font-size: 0.95rem; }
.player-tag { font-size: 1rem; }
.player-status { font-size: 1.1rem; }
</style>
