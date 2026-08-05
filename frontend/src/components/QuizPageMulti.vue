<script setup>
import { ref, computed, onMounted, onUnmounted, nextTick, watch } from 'vue'
import { useI18n } from '../composables/useI18n.js'
import { useGameWs } from '../composables/useGameWs.js'

const { t } = useI18n()
const ws = useGameWs()
const props = defineProps({ config: Object })
const emit = defineEmits(['finish'])

// Core state
const question = ref('')
const answer = ref('')
const result = ref('')
const score = ref(0)
const total = ref(0)
const finished = ref(false)
const loading = ref(false)
const answerInput = ref(null)
const phase = ref('input') // 'input' | 'evaluation'
const questionsDone = ref(0)
const countdown = ref(null)
const playerAnswered = ref(false)

// Timer refs
const timerMs = ref(0)
const timeRemainingMs = ref(0)
const timeLimitMs = computed(() => ({ 1: 8000, 2: 16000, 3: 24000 })[props.config.diff] || 8000)
let timerInterval = null
let questionTimerInterval = null

// Computed
const parsedQuestion = computed(() => {
  const q = question.value
  if (!q) return null
  const ops = [' + ', ' - ', ' * ', ' / ']
  for (const op of ops) {
    const idx = q.indexOf(op)
    if (idx !== -1) return {
      x: q.substring(0, idx), op: q.substring(idx + 1, idx + 2),
      y: q.substring(idx + 3), isDiv: q.indexOf(" / ") !== -1
    }
  }
  return null
})

const isVertical = computed(() => props.config.vDisplay === 'vertical')

const correctAnswer = computed(() => {
  const p = parsedQuestion.value
  if (!p) return null
  const x = parseInt(p.x); const y = parseInt(p.y)
  if (isNaN(x) || isNaN(y)) return null
  if (p.isDiv) return Math.floor(x / y) + ' ' + (x % y)
  if (p.op === '+') return String(x + y)
  if (p.op === '-') return String(x - y)
  if (p.op === '*') return String(x * y)
  return null
})

// Sorted leaderboard by cumulative time (lowest = fastest = best)
const sortedPlayers = computed(() => {
  return [...ws.players.value].sort((a, b) => (a.time_ms || 0) - (b.time_ms || 0))
})

function leaderTime(p) {
  return (p.time_ms || 0) / 1000
}

function gap(p) {
  const sorted = sortedPlayers.value
  if (sorted.length === 0) return 0
  return leaderTime(p) - leaderTime(sorted[0])
}

function medalFor(p) {
  const idx = sortedPlayers.value.indexOf(p)
  if (idx === 0) return '🥇'
  if (idx === 1) return '🥈'
  if (idx === 2) return '🥉'
  return (idx + 1) + '.'
}

// Countdown
function startCountdown() {
  countdown.value = 3
  const interval = setInterval(() => {
    countdown.value--
    if (countdown.value === 0) {
      clearInterval(interval)
      setTimeout(() => { countdown.value = null }, 700)
    }
  }, 1000)
}

// Reset state for a new question
function resetQuestionState() {
  answer.value = ''
  result.value = ''
  phase.value = 'input'
  playerAnswered.value = false

  // Reset and start timers
  timerMs.value = 0
  if (timerInterval) clearInterval(timerInterval)
  timerInterval = setInterval(() => { timerMs.value += 50 }, 50)

  timeRemainingMs.value = timeLimitMs.value
  if (questionTimerInterval) clearInterval(questionTimerInterval)
  questionTimerInterval = setInterval(() => {
    timeRemainingMs.value -= 50
    if (timeRemainingMs.value <= 0) {
      timeRemainingMs.value = 0
      clearInterval(questionTimerInterval)
      questionTimerInterval = null
      if (!playerAnswered.value) timeoutSubmit()
    }
  }, 50)

  nextTick(() => answerInput.value?.focus())
}

function submitAnswer() {
  if (!answer.value.trim() || playerAnswered.value || phase.value !== 'input') return
  playerAnswered.value = true
  if (timerInterval) { clearInterval(timerInterval); timerInterval = null }
  if (questionTimerInterval) { clearInterval(questionTimerInterval); questionTimerInterval = null }
  ws.submitAnswer(answer.value.trim())
}

function timeoutSubmit() {
  if (playerAnswered.value) return
  playerAnswered.value = true
  if (timerInterval) { clearInterval(timerInterval); timerInterval = null }
  if (questionTimerInterval) { clearInterval(questionTimerInterval); questionTimerInterval = null }
  ws.submitAnswer('')
}

function onKeydown(e) { if (e.key === 'Enter') submitAnswer() }

onMounted(() => {
  loading.value = true
  let countdownShown = false

  watch(() => ws.gamePhase.value, (phaseVal) => {
    if (phaseVal === 'playing' && !countdownShown) {
      countdownShown = true
      startCountdown()
    } else if (phaseVal === 'finished') {
      finished.value = true
      emit('finish', { multiplayer: true, players: ws.results.value })
    }
    // Sync local phase
    if (phaseVal === 'playing') phase.value = 'input'
    else if (phaseVal === 'evaluation') phase.value = 'evaluation'
  }, { immediate: true })

  // Wait for first question via WS
  const unwatch = watch(() => ws.question.value, (q) => {
    if (q && loading.value) {
      loading.value = false
      question.value = q
      total.value = ws.total.value
      questionsDone.value = ws.qNum.value
      resetQuestionState()
      unwatch()
    }
  })

  // Track subsequent questions
  watch(() => ws.qNum.value, (n) => {
    if (n > 0 && !loading.value) {
      questionsDone.value = n
      question.value = ws.question.value
      total.value = ws.total.value
      resetQuestionState()
    }
  })
})

onUnmounted(() => {
  if (timerInterval) clearInterval(timerInterval)
  if (questionTimerInterval) clearInterval(questionTimerInterval)
})
</script>

<template>
  <div class="card">
    <!-- Race countdown overlay (before first question) -->
    <div v-if="countdown !== null" class="countdown-overlay">
      <div class="countdown-number" :key="countdown">
        {{ countdown > 0 ? countdown : t('quiz.countdown.go') }}
      </div>
    </div>

    <div v-if="loading">{{ t('quiz.loading') }}</div>
    <div v-else-if="finished"><h2>{{ t('quiz.finished') }}</h2><p>{{ t('quiz.waiting') }}</p></div>

    <div v-else>
      <!-- Leaderboard -->
      <div class="leaderboard">
        <div class="lb-header">
          <span class="lb-title">🏁 {{ t('quiz.leaderboard') }} — Q{{ questionsDone }}/{{ total }}</span>
          <span class="lb-eval-badge" :class="{ 'lb-hidden': phase !== 'evaluation' }">{{ t('quiz.evaluating') }}</span>
        </div>
        <div class="lb-timer">
          {{ phase === 'input' ? (timerMs / 1000).toFixed(2) + 's' : '---' }}
        </div>
        <div v-for="p in sortedPlayers" :key="p.user_id" class="lb-row"
          :class="{ 'lb-waiting': !p.answered }">
          <span class="lb-medal">{{ medalFor(p) }}</span>
          <span class="lb-name">{{ p.username }}</span>
          <span class="lb-gap">{{ p.answered ? (gap(p) > 0 ? '+' + gap(p).toFixed(1) + 's' : '—') : '' }}</span>
          <span class="lb-time">{{ p.answered ? leaderTime(p).toFixed(1) + 's' : '...' }}</span>
        </div>
      </div>

      <!-- Countdown bar (question timer) -->
      <div class="countdown-bar" :class="{ 'lb-crit': timeRemainingMs <= 3000 && phase === 'input' }">
        ⏱ {{ (timeRemainingMs / 1000).toFixed(1) }}s
      </div>

      <!-- Question -->
      <div v-if="!isVertical && parsedQuestion" class="question-text">{{ parsedQuestion.x }} {{ parsedQuestion.op }} {{ parsedQuestion.y }}</div>
      <div v-else-if="isVertical && parsedQuestion" class="question-vertical">
        <div class="qv-operator">{{ parsedQuestion.op }}</div>
        <div class="qv-numbers">
          <div class="qv-x">{{ parsedQuestion.x }}</div>
          <div class="qv-y">{{ parsedQuestion.y }}</div>
        </div>
      </div>

      <!-- Division hint -->
      <div class="division-hint-wrapper">
        <div v-if="parsedQuestion?.isDiv" class="division-hint">
          {{ t('quiz.div.hint') }}<br/>
          {{ t('quiz.div.example') }}: 11 ÷ 7 → <kbd>1 4</kbd>
        </div>
      </div>

      <!-- Answer input -->
      <input ref="answerInput" class="answer-input" v-model="answer"
        :disabled="playerAnswered || phase !== 'input'"
        :placeholder="t('quiz.placeholder')" @keydown="onKeydown" autofocus />
      <button class="btn btn-primary" @click="submitAnswer"
        :disabled="!answer.trim() || playerAnswered || phase !== 'input'">
        {{ t('quiz.submit') }}
      </button>

      <!-- Status cards below input: player names, answers, verdicts for current round -->
      <div class="mp-players">
        <div v-for="p in ws.players.value" :key="p.user_id" class="ai-section"
          :class="{ 'mp-dimmed': phase === 'input' && !p.answered }">
          <div class="ai-result-row">
            <span class="ai-answer">{{ p.username }} ({{ p.correct }}/{{ total }})</span>
            <span class="ai-verdict" v-if="phase === 'evaluation' && p.answered"
              :class="p.correct ? 'result-correct' : 'result-wrong'">
              {{ p.correct ? t('quiz.correct') : t('quiz.wrong') }}
              ({{ p.time_ms > 0 ? (p.time_ms / 1000).toFixed(1) + 's' : '?' }})
            </span>
            <span v-else-if="!p.answered && !playerAnswered" class="ai-thinking">{{ t('quiz.ai.thinking') }}</span>
            <span v-else-if="!p.answered && playerAnswered" class="ai-thinking">{{ t('quiz.submitted') }}</span>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.score-row { display: flex; flex-wrap: wrap; gap: 0.8rem; align-items: center; justify-content: space-between; }
.score-text { font-size: 0.95rem; color: #ccc; }
.score-text strong { color: #fff; font-size: 1.05rem; }
.ai-score { color: #48c6ef; font-size: 0.9rem; }
.question-vertical { display: flex; align-items: center; justify-content: center; gap: 1.5rem; margin: 1.5rem 0; font-family: 'Courier New', monospace; }
.qv-operator { font-size: 2.5rem; font-weight: 700; color: #b0b0ff; }
.qv-numbers { display: flex; flex-direction: column; align-items: flex-end; font-size: 2.5rem; font-weight: 700; color: #fff; }
.qv-x, .qv-y { padding: 0.1rem 0; }
.division-hint { font-size: 0.85rem; color: #b0b0ff; margin-bottom: 0.8rem; line-height: 1.5; }
.division-hint kbd { background: rgba(108, 99, 255, 0.2); border: 1px solid rgba(108, 99, 255, 0.4); border-radius: 4px; padding: 0.1rem 0.4rem; font-family: 'Courier New', monospace; font-size: 0.9rem; }
.human-section { margin-top: 0.5rem; padding: 0.4rem 0.5rem; border-radius: 8px; background: rgba(255,215,0,0.08); border: 1px solid rgba(255,215,0,0.2); }
.human-result-row { display: flex; align-items: center; gap: 0.5rem; }
.human-answer { color: #ffd700; font-size: 0.9rem; }
.human-answer strong { font-family: 'Courier New', monospace; font-size: 1.1rem; }
.human-verdict { font-size: 0.85rem; font-weight: 600; }
.human-waiting { color: #ffd700; font-size: 0.85rem; animation: pulse 1.5s ease-in-out infinite; }
.mp-players { margin-top: 0.5rem; }
.mp-highlight { border-color: rgba(255,215,0,0.4) !important; }
.mp-correct { border-color: rgba(72, 199, 142, 0.5) !important; background: rgba(72, 199, 142, 0.1) !important; }
.mp-wrong { border-color: rgba(255, 68, 68, 0.5) !important; background: rgba(255, 68, 68, 0.1) !important; }
.mp-dimmed { opacity: 0.4; }
.ai-section { margin-top: 0.5rem; padding: 0.4rem 0.5rem; border-radius: 8px; background: rgba(72, 198, 239, 0.08); border: 1px solid rgba(72, 198, 239, 0.15); }
.ai-thinking { color: #48c6ef; font-size: 0.85rem; animation: pulse 1.5s ease-in-out infinite; }
@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
.ai-revealed .ai-answer { color: #48c6ef; font-size: 0.9rem; }
.ai-revealed .ai-answer strong { font-family: 'Courier New', monospace; font-size: 1.1rem; }
.ai-result-row { display: flex; align-items: center; gap: 0.5rem; }
.ai-verdict { font-size: 0.85rem; font-weight: 600; }
.leaderboard { background: rgba(0,0,0,0.25); border-radius: 10px; padding: 0.6rem; margin-bottom: 1rem; min-height: 12.5rem; }
.lb-header { display: flex; align-items: center; justify-content: space-between; }
.lb-title { font-size: 0.85rem; font-weight: 600; color: #ffd700; }
.lb-eval-badge { font-size: 0.75rem; background: rgba(255,215,0,0.15); color: #ffd700; padding: 0.1rem 0.5rem; border-radius: 4px; animation: pulse 1s infinite; }
.lb-eval-banner { text-align: center; font-size: 0.9rem; color: #ffd700; padding: 0.3rem 0; animation: pulse 1s infinite; }
.lb-hidden { visibility: hidden; }
.division-hint-wrapper { min-height: 3.5rem; }
.lb-timer { font-family: 'Courier New', monospace; font-size: 1.6rem; font-weight: 700; color: #fff; text-align: center; padding: 0.2rem 0 0.4rem 0; }
.lb-timer-flash { animation: timer-flash 0.6s ease-out; }
.countdown-bar { font-family: 'Courier New', monospace; font-size: 1.3rem; font-weight: 700; color: #ffd700; text-align: center; padding: 0.5rem 0; margin-bottom: 0.5rem; background: rgba(0,0,0,0.15); border-radius: 8px; transition: color 0.3s; }
.lb-crit { color: #ff4444; animation: countdown-crit 1s ease-in-out infinite; }
@keyframes countdown-crit { 0%, 100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.4; transform: scale(1.08); } }
@keyframes timer-flash { 0% { transform: scale(1.6); } 50% { transform: scale(1.3); } 100% { transform: scale(1); } }
.lb-row { display: flex; align-items: center; gap: 0.4rem; padding: 0.25rem 0; font-size: 0.9rem; border-radius: 6px; overflow: hidden; }
.lb-row-flash { animation: row-flash 0.6s ease-out; }
@keyframes row-flash { 0% { background: rgba(255,255,255,0.08); } 100% { background: transparent; } }
.lb-medal { width: 1.6rem; text-align: center; flex-shrink: 0; }
.lb-name { flex: 1; color: #ccc; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.lb-human { color: #fff; font-weight: 600; }
.lb-time { font-family: 'Courier New', monospace; color: #fff; width: 3.5rem; text-align: right; flex-shrink: 0; }
.lb-gap { font-family: 'Courier New', monospace; color: #888; width: 3.5rem; text-align: right; flex-shrink: 0; }
.lb-waiting { opacity: 0.45; }

/* Need position:relative on card for absolute countdown overlay */
.card { position: relative; }

.countdown-overlay { position: absolute; inset: 0; background: rgba(0,0,0,0.85); display: flex; align-items: center; justify-content: center; z-index: 10; border-radius: 10px; }
.countdown-number { font-size: 5rem; font-weight: 700; color: #fff; animation: countdown-pop 0.6s ease-out; }
@keyframes countdown-pop { 0% { transform: scale(2.5); opacity: 0; } 40% { transform: scale(1); opacity: 1; } 100% { transform: scale(0.9); opacity: 0.8; } }
</style>
