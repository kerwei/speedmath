<script setup>
import { ref, computed, onMounted, onUnmounted, nextTick, watch } from 'vue'
import { useI18n } from '../composables/useI18n.js'
import { useGameWs } from '../composables/useGameWs.js'

const { t } = useI18n()
const ws = useGameWs()
const props = defineProps({ config: Object, multiplayer: Boolean })
const emit = defineEmits(['finish'])

// Core state
const question = ref('')
const answer = ref('')
const result = ref('')
const score = ref(0)
const total = ref(0)
const sessionId = ref('')
const finished = ref(false)
const loading = ref(false)
const answerInput = ref(null)
const phase = ref('input') // 'input' | 'evaluation' | 'advancing'
const questionsDone = ref(0)

// AI state
const aiCount = ref(0)
const aiAnswers = ref([])
const aiDelays = ref([])
const aiScores = ref([])
const aiCorrects = ref([])
const aiFinished = ref([])
const aiRevealed = ref([])
const aiSubmitted = ref([])
const aiPhase = ref([])
let aiTimers = []
let aiPhaseTimers = []

// Countdown state (null = hidden, 3/2/1 = counting, 0 = "Go!")
const countdown = ref(null)

// Question time limit (Easy=8s, Medium=16s, Hard=24s)
const timeLimitMs = computed(() => ({ 1: 8000, 2: 16000, 3: 24000 })[props.config.diff] || 8000)
const timeRemainingMs = ref(0)
const timeoutTriggered = ref(false)
let questionTimerInterval = null

// Timer
const timerMs = ref(0)
const flashPlayer = ref(null) // index that just submitted, for flash
let timerInterval = null

// Leaderboard state during input phase
const standings = ref([]) // official cumulative times (with penalties from API)
const baseTimes = ref([]) // snapshot of standings at round start (before this Q)
const rawTimes = ref([])  // this round's raw time per player (null = not submitted)
const submissionOrder = ref([]) // player indices in submission order

// Official leaderboard (after penalties, from API)
const leaderboardOfficial = computed(() => {
  return standings.value.map((s, i) => ({
    idx: i, name: playerName(i), time: s.t, correct: s.c
  })).sort((a, b) => a.time - b.time)
})

// Raw leaderboard — always shows all players (previous standings until they submit)
const leaderboardRaw = computed(() => {
  const entries = []
  for (let i = 0; i < (1 + aiCount.value); i++) {
    if (rawTimes.value[i] !== null) {
      entries.push({
        idx: i, name: playerName(i),
        time: baseTimes.value[i] + rawTimes.value[i],
        waiting: false
      })
    } else if (baseTimes.value.length > 0) {
      entries.push({
        idx: i, name: playerName(i),
        time: baseTimes.value[i],
        waiting: true
      })
    }
  }
  entries.sort((a, b) => a.time - b.time)
  if (entries.length === 0) return entries
  const leaderTime = entries[0].time
  entries.forEach(e => { e.gap = e.time - leaderTime })
  return entries
})

// Active leaderboard based on phase
const activeLeaderboard = computed(() => {
  return leaderboardRaw.value
})

function playerName(i) {
  return i === 0 ? t('human') : `${t('ai.prefix')}${i}`
}

function medalFor(entry) {
  if (entry.waiting) return '⌛'
  const idx = activeLeaderboard.value.indexOf(entry)
  if (idx === 0) return '🥇'
  if (idx === 1) return '🥈'
  if (idx === 2) return '🥉'
  return (idx + 1) + '.'
}

const progress = computed(() =>
  total.value > 0 ? (score.value / total.value) * 100 : 0
)

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

function computeAiCorrect(aiIdx) {
  const p = parsedQuestion.value
  if (!p) return false
  const x = parseInt(p.x); const y = parseInt(p.y)
  const ans = aiAnswers.value[aiIdx]
  if (!ans) return false
  if (p.isDiv) {
    const parts = ans.split(' ')
    if (parts.length === 2) return (parseInt(parts[0]) * y + parseInt(parts[1]) === x)
    return false
  }
  if (p.op === '+') return parseInt(ans) === x + y
  if (p.op === '-') return parseInt(ans) === x - y
  if (p.op === '*') return parseInt(ans) === x * y
  return false
}

async function apiCall(endpoint, data) {
  try {
    const params = new URLSearchParams(data)
    const res = await fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: params
    })
    if (!res.ok) {
      console.error('[API]', endpoint, res.status, res.statusText)
      return {}
    }
    const json = await res.json()
    console.log('[API]', endpoint, '→', JSON.stringify(json).substring(0, 120))
    return json
  } catch (e) {
    console.error('[API]', endpoint, 'ERROR:', e.message)
    return {}
  }
}

function initAiArrays(n) {
  aiAnswers.value = new Array(n).fill('')
  aiDelays.value = new Array(n).fill(0)
  aiScores.value = new Array(n).fill(0)
  aiCorrects.value = new Array(n).fill(false)
  aiFinished.value = new Array(n).fill(false)
  aiRevealed.value = new Array(n).fill(false)
  aiSubmitted.value = new Array(n).fill(false)
  aiPhase.value = new Array(n).fill('')
  aiTimers = []; aiPhaseTimers = []
}

function recordSubmission(pidx, rawTimeMs) {
  rawTimes.value[pidx] = rawTimeMs
  submissionOrder.value.push(pidx)
  flashPlayer.value = pidx
  setTimeout(() => { if (flashPlayer.value === pidx) flashPlayer.value = null }, 700)

  // Check if all players have submitted
  const totalPlayers = 1 + aiCount.value
  if (rawTimes.value.filter(t => t !== null).length >= totalPlayers) {
    // All done → reveal answers and enter evaluation phase
    revealAll()
    phase.value = 'evaluation'
    // Advance after a brief delay
    setTimeout(() => {
      phase.value = 'advancing'
      if (!playerAnswered.value) return
      setTimeout(() => nextQuestion(), 400)
    }, 800)
  }
}

function revealAll() {
  for (let i = 0; i < aiCount.value; i++) {
    aiRevealed.value[i] = true
  }
}

async function newGame() {
  try {
    loading.value = true
    const data = await apiCall('/api/game/new', {
      diff: props.config.diff, intense: props.config.intense,
      ops: props.config.ops, ai_levels: props.config.ai_levels || ''
    })
    if (!data.session_id) { console.error('[Quiz] no session_id from /api/game/new'); return }
    sessionId.value = data.session_id
    total.value = props.config.intense * 10
    const levelsStr = props.config.ai_levels || ''
    const levelCount = levelsStr ? levelsStr.split(',').filter(s => s).length : 0
    aiCount.value = levelCount
    initAiArrays(levelCount)
    loading.value = false
    await nextTick()
    await nextQuestion()
  } catch (e) {
    console.error('[Quiz] newGame ERROR:', e.message)
    loading.value = false
  }
}

async function nextQuestion() {
  try {
    const data = await apiCall('/api/game/question', { session_id: sessionId.value })
    if (!data || data.finished) { finished.value = true; emit('finish', sessionId.value); return }
    if (!data.question) { console.error('[Quiz] no question in response'); return }

    question.value = data.question; answer.value = ''; result.value = ''
    phase.value = 'input'
    playerAnswered.value = false
    flashPlayer.value = null

    if (data.standings) standings.value = data.standings
    baseTimes.value = (data.standings || []).map(s => s.t)
    const totalPlayers = 1 + (data.ai_answers ? data.ai_answers.length : 0)
    rawTimes.value = new Array(totalPlayers).fill(null)
    submissionOrder.value = []

    aiTimers.forEach(t => clearTimeout(t)); aiPhaseTimers.forEach(t => clearTimeout(t))
    aiTimers = []; aiPhaseTimers = []
    for (let i = 0; i < aiCount.value; i++) {
      aiFinished.value[i] = false; aiRevealed.value[i] = false
      aiCorrects.value[i] = false; aiPhase.value[i] = ''
      aiSubmitted.value[i] = false
    }

    if (data.questions !== undefined) questionsDone.value = data.questions

    timerMs.value = 0
    if (timerInterval) clearInterval(timerInterval)
    timerInterval = setInterval(() => { timerMs.value += 50 }, 50)

    // Start question countdown
    timeRemainingMs.value = timeLimitMs.value
    timeoutTriggered.value = false
    if (questionTimerInterval) clearInterval(questionTimerInterval)
    questionTimerInterval = setInterval(() => {
      timeRemainingMs.value -= 50
      if (timeRemainingMs.value <= 0) {
        timeRemainingMs.value = 0
        clearInterval(questionTimerInterval)
        questionTimerInterval = null
        if (!playerAnswered.value && !timeoutTriggered.value) {
          timeoutTriggered.value = true
          timeoutSubmit()
        }
      }
    }, 50)

    if (data.ai_answers && data.ai_delays) {
      const n = Math.min(data.ai_answers.length, data.ai_delays.length)
      for (let i = 0; i < n; i++) {
        aiAnswers.value[i] = data.ai_answers[i]
        aiDelays.value[i] = parseInt(data.ai_delays[i]) || 2000
        aiPhase.value[i] = 'reading'
        aiPhaseTimers.push(setTimeout(() => { aiPhase.value[i] = 'thinking' }, 700))
        const timer = setTimeout(() => {
          const correct = computeAiCorrect(i)
          if (correct) aiScores.value[i]++
          aiCorrects.value[i] = correct
          aiFinished.value[i] = true; aiSubmitted.value[i] = true
          recordSubmission(1 + i, aiDelays.value[i])
        }, aiDelays.value[i])
        aiTimers.push(timer)
      }
    }

    await nextTick()
    answerInput.value?.focus()
  } catch (e) {
    console.error('[Quiz] nextQuestion ERROR:', e.message)
  }
}

const playerAnswered = ref(false)

function timeoutSubmit() {
  playerAnswered.value = true
  if (timerInterval) { clearInterval(timerInterval); timerInterval = null }

  // Record with full time limit as raw time
  recordSubmission(0, timeLimitMs.value)

  // Submit empty answer to API (server marks as wrong)
  apiCall('/api/game/answer', { session_id: sessionId.value, answer: '' }).then(data => {
    result.value = data.result || 'wrong'
    if (data.result === 'correct') score.value++
  })
}

async function submitAnswer() {
  if (!answer.value.trim()) return

  if (props.multiplayer) {
    // 多人模式 — 通过 WebSocket 提交 (tōngguò WebSocket tíjiāo — submit via WS)
    playerAnswered.value = true
    ws.submitAnswer(answer.value.trim())
    return
  }

  playerAnswered.value = true

  // Clear timers
  if (timerInterval) clearInterval(timerInterval); timerInterval = null
  if (questionTimerInterval) { clearInterval(questionTimerInterval); questionTimerInterval = null }
  const humanTime = timerMs.value

  // Record human submission (before API call for immediate feedback)
  recordSubmission(0, humanTime)

  // API call to grade & penalize
  const data = await apiCall('/api/game/answer', { session_id: sessionId.value, answer: answer.value })
  result.value = data.result
  if (data.result === 'correct') score.value++
}

function onKeydown(e) { if (e.key === 'Enter') submitAnswer() }

function startCountdown() {
  countdown.value = 3
  const interval = setInterval(() => {
    countdown.value--
    if (countdown.value === 0) {
      clearInterval(interval)
      // Show "Go!" for 700ms, then start game
      setTimeout(() => {
        countdown.value = null
        newGame()
      }, 700)
    }
  }, 1000)
}

onMounted(() => {
  if (props.multiplayer) {
    // 多人模式 — 通过 WebSocket 接收题目 (jiēshōu tímù — receive questions via WS)
    loading.value = true
    // Wait for first question via WS
    const unwatch = watch(() => ws.question.value, (q) => {
      if (q && loading.value) {
        loading.value = false
        question.value = q
        total.value = ws.total.value
        questionsDone.value = ws.qNum.value - 1
        phase.value = 'input'
        unwatch()
      }
    })
    // Also listen for game_over
    watch(() => ws.gamePhase.value, (phase) => {
      if (phase === 'finished') {
        finished.value = true
        emit('finish', { multiplayer: true, players: ws.results.value })
      }
    })
    // Track questions
    watch(() => ws.qNum.value, (n) => {
      if (n > 0) {
        questionsDone.value = n - 1
        question.value = ws.question.value
        total.value = ws.total.value
        phase.value = 'input'
        answer.value = ''
        result.value = ''
      }
    })
  } else {
    startCountdown()
  }
})
onUnmounted(() => {
  if (timerInterval) clearInterval(timerInterval)
  if (questionTimerInterval) clearInterval(questionTimerInterval)
  aiTimers.forEach(t => clearTimeout(t)); aiPhaseTimers.forEach(t => clearTimeout(t))
})
</script>

<template>
  <div class="card">
    <!-- Race countdown overlay (first question only) -->
    <div v-if="countdown !== null" class="countdown-overlay">
      <div class="countdown-number" :key="countdown">
        {{ countdown > 0 ? countdown : t('quiz.countdown.go') }}
      </div>
    </div>

    <div class="score-row">
      <div class="score-text">😎 {{ score }} / {{ total }}</div>
      <div v-for="i in aiCount" :key="'ai'+i" class="score-text ai-score">🤖{{ t('ai.prefix') }}{{ i }} {{ aiScores[i-1] || 0 }}/{{ total }}</div>
    </div>
    <div class="progress-bar"><div class="progress-fill" :style="{ width: progress + '%' }"></div></div>

    <div v-if="loading">{{ t('quiz.loading') }}</div>
    <div v-else-if="finished"><h2>{{ t('quiz.finished') }}</h2><p>{{ t('quiz.waiting') }}</p></div>

    <div v-else>
      <!-- Leaderboard -->
      <div class="leaderboard">
        <div class="lb-header">
          <span class="lb-title">🏁 {{ t('quiz.leaderboard') }} — Q{{ questionsDone }}/{{ total }}</span>
          <span class="lb-eval-badge" :class="{ 'lb-hidden': phase !== 'evaluation' }">{{ t('quiz.evaluating') }}</span>
        </div>
        <div class="lb-timer" :class="{ 'lb-timer-flash': flashPlayer === 0 && phase === 'input' }">
          {{ phase === 'input' ? (timerMs / 1000).toFixed(2) + 's' : (rawTimes[0] !== null ? (rawTimes[0] / 1000).toFixed(2) + 's' : '---') }}
        </div>
        <div class="lb-eval-banner" :class="{ 'lb-hidden': phase !== 'evaluation' }">{{ t('quiz.evaluating') }}</div>
        <!-- Player rows: all players shown, waiting ones dimmed -->
        <div v-for="entry in activeLeaderboard" :key="entry.idx" class="lb-row"
          :class="{ 'lb-row-flash': flashPlayer === entry.idx && phase === 'input', 'lb-waiting': entry.waiting }">
          <span class="lb-medal">{{ medalFor(entry) }}</span>
          <span class="lb-name" :class="{ 'lb-human': entry.idx === 0 }">{{ entry.name }}</span>
          <span class="lb-gap">{{ entry.waiting ? '' : (entry.gap > 0 ? '+' + (entry.gap / 1000).toFixed(1) + 's' : '—') }}</span>
          <span class="lb-time">{{ (entry.time / 1000).toFixed(1) }}s</span>
        </div>
      </div>

      <!-- Countdown (between leaderboard and question) -->
      <div class="countdown-bar" :class="{ 'lb-crit': timeRemainingMs <= 3000 && phase === 'input' }">
        ⏱ {{ (timeRemainingMs / 1000).toFixed(1) }}s
      </div>

      <!-- Question -->
      <div v-if="!isVertical && parsedQuestion" class="question-text">{{ parsedQuestion.x }} {{ parsedQuestion.op }} {{ parsedQuestion.y }}</div>
      <div v-else-if="isVertical && parsedQuestion" class="question-vertical">
        <div class="qv-operator">{{ parsedQuestion.op }}</div>
        <div class="qv-numbers"><div class="qv-x">{{ parsedQuestion.x }}</div><div class="qv-y">{{ parsedQuestion.y }}</div></div>
      </div>

      <div class="division-hint-wrapper">
        <div v-if="parsedQuestion?.isDiv" class="division-hint">
          {{ t('quiz.div.hint') }}<br/>
          {{ t('quiz.div.example') }}: 11 ÷ 7 → <kbd>1 4</kbd>
        </div>
      </div>

      <!-- Input (disabled after human submits or during evaluation) -->
      <input ref="answerInput" class="answer-input" v-model="answer" :disabled="playerAnswered || phase !== 'input'"
        :placeholder="t('quiz.placeholder')" @keydown="onKeydown" autofocus />
      <button class="btn btn-primary" @click="submitAnswer"
        :disabled="!answer.trim() || playerAnswered || phase !== 'input'">{{ t('quiz.submit') }}</button>

      <!-- 多人模式玩家列表 (duōrén móshì wánjiā lièbiǎo — multiplayer player list) -->
      <div v-if="props.multiplayer" class="mp-players">
        <div v-for="p in ws.players.value" :key="p.user_id" class="ai-section"
          :class="{ 'mp-highlight': ws.gamePhase.value === 'finished' }">
          <div class="ai-result-row">
            <span class="ai-answer">{{ p.username }}: <strong>{{ p.answered ? (p.correct + '/' + ws.total.value) : '...' }}</strong></span>
            <span class="ai-verdict" v-if="p.answered && ws.gamePhase.value === 'evaluation'"
              :class="'result-' + (p.correct > 0 ? 'correct' : 'wrong')">
              ({{ p.time_ms > 0 ? (p.time_ms / 1000).toFixed(1) + 's' : '?' }})
            </span>
          </div>
        </div>
      </div>

      <!-- Human result card (always visible, same style as AI) -->
      <div v-if="!props.multiplayer" class="human-section">
        <!-- Evaluation: show answer + verdict -->
        <div v-if="phase === 'evaluation' && result" class="human-result-row">
          <span class="human-answer">😎 {{ t('human') }}: <strong>{{ answer || '—' }}</strong></span>
          <span class="human-verdict" :class="result === 'correct' ? 'result-correct' : 'result-wrong'">({{ result === 'correct' ? t('quiz.correct') : t('quiz.wrong') }})</span>
        </div>
        <!-- Submitted, waiting for others -->
        <div v-else-if="playerAnswered" class="human-waiting">😎 {{ t('human') }} {{ t('quiz.submitted') }}</div>
        <!-- Not yet submitted -->
        <div v-else class="human-waiting">😎 {{ t('human') }} {{ t('quiz.ai.thinking') }}</div>
      </div>

      <!-- AI result cards -->
      <div v-if="!props.multiplayer" class="ai-section-wrapper">
        <div v-for="i in aiCount" :key="'ai-state-'+i" class="ai-section">
          <!-- Evaluation: show answer + verdict -->
          <div v-if="aiRevealed[i-1]" class="ai-revealed">
            <div class="ai-result-row">
              <span class="ai-answer">🤖{{ t('ai.prefix') }}{{ i }}: <strong>{{ aiAnswers[i-1] }}</strong></span>
              <span class="ai-verdict" :class="aiCorrects[i-1] ? 'result-correct' : 'result-wrong'">({{ aiCorrects[i-1] ? t('quiz.correct') : t('quiz.wrong') }})</span>
            </div>
          </div>
          <!-- Submitted, waiting for others -->
          <div v-else-if="aiSubmitted[i-1]" class="ai-thinking">🤖{{ t('ai.prefix') }}{{ i }} {{ t('quiz.ai.answered') }}</div>
          <!-- Reading / thinking -->
          <div v-else-if="aiPhase[i-1] === 'reading'" class="ai-thinking">🤖{{ t('ai.prefix') }}{{ i }} {{ t('quiz.ai.reading') }}</div>
          <div v-else-if="aiPhase[i-1] === 'thinking'" class="ai-thinking">🤖{{ t('ai.prefix') }}{{ i }} {{ t('quiz.ai.thinking') }}</div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.score-row { display: flex; flex-wrap: wrap; gap: 0.8rem; align-items: center; justify-content: space-between; }
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
