<script setup>
import { ref, onMounted } from 'vue'
import { useI18n } from '../composables/useI18n.js'

const { t } = useI18n()
const props = defineProps({ sessionId: String, mpPlayers: { type: Array, default: () => [] } })
const emit = defineEmits(['restart'])

const players = ref([])
const loading = ref(true)

const DIFFICULTY_KEY = ['results.easy', 'results.medium', 'results.hard']

function typeLabel(p) {
  if (p.type === 'human') return t('human')
  const diffKey = DIFFICULTY_KEY[p.level] || DIFFICULTY_KEY[0]
  return t('ai.prefix') + p.idx + ' (' + t(diffKey) + ')'
}

onMounted(async () => {
  // If WS multiplayer results are provided directly, use them
  if (props.mpPlayers && props.mpPlayers.length > 0) {
    players.value = [...props.mpPlayers].sort((a, b) => a.time_ms - b.time_ms)
    loading.value = false
    return
  }
  // Otherwise fetch from REST API
  const params = new URLSearchParams({ session_id: props.sessionId })
  const res = await fetch(`/api/game/results?${params}`)
  const data = await res.json()
  if (data.players) {
    // Sort by cumulative time ascending
    data.players.sort((a, b) => a.time_ms - b.time_ms)
    players.value = data.players
  }
  loading.value = false
})
</script>

<template>
  <div class="card">
    <h2>{{ t('results.title') }}</h2>

    <div v-if="loading">{{ t('results.loading') }}</div>

    <div v-else>
      <h3 class="lb-title">🏁 {{ t('results.leaderboard') }}</h3>

      <div class="results-table">
        <div class="rt-row rt-header">
          <span class="rt-rank">#</span>
          <span class="rt-name">{{ t('results.difficulty') }}</span>
          <span class="rt-correct">{{ t('results.correct') }}</span>
          <span class="rt-time">{{ t('results.time') }}</span>
          <span class="rt-gap">{{ t('results.gap') }}</span>
        </div>
        <div v-for="(p, i) in players" :key="p.idx" class="rt-row"
          :class="{ 'rt-human': p.type === 'human' }">
          <span class="rt-rank">{{ i === 0 ? '🥇' : i === 1 ? '🥈' : i === 2 ? '🥉' : (i + 1) + '.' }}</span>
          <span class="rt-name">{{ typeLabel(p) }}</span>
          <span class="rt-correct">{{ p.correct }}/{{ p.total }}</span>
          <span class="rt-time">{{ (p.time_ms / 1000).toFixed(1) }}s</span>
          <span class="rt-gap">{{ i === 0 ? '—' : '+' + ((p.time_ms - players[0].time_ms) / 1000).toFixed(1) + 's' }}</span>
        </div>
      </div>
    </div>

    <button class="btn btn-primary" @click="emit('restart')" style="margin-top: 1.5rem">
      {{ t('results.playagain') }}
    </button>
  </div>
</template>

<style scoped>
.lb-title { color: #ffd700; margin: 0.5rem 0 1rem; font-size: 1.1rem; }
.results-table { background: rgba(0,0,0,0.25); border-radius: 10px; padding: 0.6rem; }
.rt-row { display: grid; grid-template-columns: 2rem 1fr 4rem 4.5rem 4.5rem; gap: 0.3rem; align-items: center; padding: 0.3rem 0; font-size: 0.9rem; }
.rt-header { font-weight: 600; color: #888; font-size: 0.8rem; border-bottom: 1px solid rgba(255,255,255,0.1); padding-bottom: 0.4rem; margin-bottom: 0.2rem; }
.rt-human { background: rgba(255,215,0,0.08); border-radius: 6px; }
.rt-rank { text-align: center; }
.rt-name { color: #ccc; }
.rt-human .rt-name { color: #fff; font-weight: 600; }
.rt-correct, .rt-time, .rt-gap { font-family: 'Courier New', monospace; text-align: right; color: #fff; }
.rt-gap { color: #888; }
</style>
