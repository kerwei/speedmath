<script setup>
import { ref, computed, onMounted, onUnmounted, nextTick, watch } from 'vue'

const props = defineProps({ config: Object })
const emit = defineEmits(['finish'])

const question = ref('')
const answer = ref('')
const result = ref('')
const score = ref(0)
const total = ref(0)
const sessionId = ref('')
const finished = ref(false)
const loading = ref(false)
const answerInput = ref(null)
const playerAnswered = ref(false)
const advancing = ref(false)

// Multi-AI state
const aiCount = ref(0)
const aiAnswers = ref([])
const aiDelays = ref([])
const aiScores = ref([])
const aiCorrects = ref([])
const aiFinished = ref([])
const aiAnswered = ref([])   // AI done but hidden
const aiRevealed = ref([])   // AI answer shown
const aiPhase = ref([])      // '', 'reading', 'thinking'
let aiTimers = []
let aiPhaseTimers = []

const maxAiScore = computed(() => {
  return aiScores.value.length > 0 ? Math.max(...aiScores.value) : 0
})

const progress = computed(() =>
  total.value > 0 ? (score.value / total.value) * 100 : 0
)

const parsedQuestion = computed(() => {
  const q = question.value
  if (!q) return null
  const ops = [' + ', ' - ', ' * ', ' / ']
  for (const op of ops) {
    const idx = q.indexOf(op)
    if (idx !== -1) {
      return {
        x: q.substring(0, idx),
        op: q.substring(idx + 1, idx + 2),
        y: q.substring(idx + 3),
        isDiv: q.indexOf(" / ") !== -1
      }
    }
  }
  return null
})

const isVertical = computed(() => props.config.vDisplay === 'vertical')

function computeAiCorrect(aiIdx) {
  const p = parsedQuestion.value
  if (!p) return false
  const x = parseInt(p.x)
  const y = parseInt(p.y)
  const ans = aiAnswers.value[aiIdx]
  if (!ans) return false
  if (p.isDiv) {
    const parts = ans.split(' ')
    if (parts.length === 2) {
      const q = parseInt(parts[0])
      const r = parseInt(parts[1])
      return (q * y + r === x)
    }
    return false
  } else if (p.op === '+') {
    return parseInt(ans) === x + y
  } else if (p.op === '-') {
    return parseInt(ans) === x - y
  } else if (p.op === '*') {
    return parseInt(ans) === x * y
  }
  return false
}

function tryAdvance() {
  if (advancing.value) return
  const allDone = playerAnswered.value && aiFinished.value.every(f => f)
  if (allDone) {
    advancing.value = true
    // Reveal all AIs
    for (let i = 0; i < aiCount.value; i++) {
      aiRevealed.value[i] = true
    }
    setTimeout(() => nextQuestion(), 600)
  }
}

async function apiCall(endpoint, data) {
  const params = new URLSearchParams(data)
  const res = await fetch(endpoint, {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: params
  })
  return res.json()
}

function initAiArrays(n) {
  aiAnswers.value = new Array(n).fill('')
  aiDelays.value = new Array(n).fill(0)
  aiScores.value = new Array(n).fill(0)
  aiCorrects.value = new Array(n).fill(false)
  aiFinished.value = new Array(n).fill(false)
  aiAnswered.value = new Array(n).fill(false)
  aiRevealed.value = new Array(n).fill(false)
  aiPhase.value = new Array(n).fill('')
  aiTimers = aiTimers.slice(0, n)
  aiPhaseTimers = aiPhaseTimers.slice(0, n)
}

async function newGame() {
  loading.value = true
  const data = await apiCall('/api/game/new', {
    diff: props.config.diff,
    intense: props.config.intense,
    ops: props.config.ops,
    ai_levels: props.config.ai_levels || ''
  })
  sessionId.value = data.session_id
  total.value = props.config.intense * 10

  const levelsStr = props.config.ai_levels || ''
  const levelCount = levelsStr ? levelsStr.split(',').filter(s => s).length : 0
  aiCount.value = levelCount
  initAiArrays(levelCount)

  loading.value = false
  await nextTick()
  await nextQuestion()
}

async function nextQuestion() {
  const data = await apiCall('/api/game/question', {
    session_id: sessionId.value
  })
  if (data.finished) {
    finished.value = true
    emit('finish', sessionId.value)
    return
  }
  question.value = data.question
  answer.value = ''
  result.value = ''
  playerAnswered.value = false
  advancing.value = false

  // Reset AI state
  for (let i = 0; i < aiCount.value; i++) {
    aiFinished.value[i] = false
    aiAnswered.value[i] = false
    aiRevealed.value[i] = false
    aiCorrects.value[i] = false
    aiPhase.value[i] = ''
  }
  // Clear existing timers
  aiTimers.forEach(t => clearTimeout(t))
  aiPhaseTimers.forEach(t => clearTimeout(t))
  aiTimers = []
  aiPhaseTimers = []

  // Setup AIs from server response
  if (data.ai_answers && data.ai_delays) {
    const n = Math.min(data.ai_answers.length, data.ai_delays.length)
    for (let i = 0; i < n; i++) {
      aiAnswers.value[i] = data.ai_answers[i]
      aiDelays.value[i] = parseInt(data.ai_delays[i]) || 2000

      // Phase 1: reading (700ms)
      aiPhase.value[i] = 'reading'
      const phaseTimer = setTimeout(() => {
        aiPhase.value[i] = 'thinking'
      }, 700)
      aiPhaseTimers.push(phaseTimer)

      // AI finishes computing
      const timer = setTimeout(() => {
        const correct = computeAiCorrect(i)
        if (correct) aiScores.value[i]++
        aiCorrects.value[i] = correct
        aiFinished.value[i] = true
        aiAnswered.value[i] = true
        tryAdvance()
      }, aiDelays.value[i])
      aiTimers.push(timer)
    }
  }

  await nextTick()
  answerInput.value?.focus()
}

async function submitAnswer() {
  if (!answer.value.trim()) return
  const data = await apiCall('/api/game/answer', {
    session_id: sessionId.value,
    answer: answer.value
  })
  result.value = data.result
  if (data.result === 'correct') {
    score.value++
  }
  playerAnswered.value = true
  tryAdvance()
}

function onKeydown(e) {
  if (e.key === 'Enter') submitAnswer()
}

onMounted(() => newGame())

onUnmounted(() => {
  aiTimers.forEach(t => clearTimeout(t))
  aiPhaseTimers.forEach(t => clearTimeout(t))
})
</script>

<template>
  <div class="card">
    <div class="score-row">
      <div class="score-text">😎 {{ score }} / {{ total }}</div>
      <div v-for="i in aiCount" :key="'ai'+i" class="score-text ai-score">
        🤖AI{{ i }} {{ aiScores[i-1] || 0 }}/{{ total }}
      </div>
    </div>
    <div class="progress-bar">
      <div class="progress-fill" :style="{ width: progress + '%' }"></div>
    </div>

    <div v-if="loading">加载中 (jiāzài zhōng — Loading)...</div>

    <div v-else-if="finished">
      <h2>题目完成！ (tímù wánchéng — All questions done!)</h2>
      <p>请等待结果... (qǐng děngdài jiéguǒ — Please wait for results)</p>
    </div>

    <div v-else>
      <div v-if="!isVertical && parsedQuestion" class="question-text">
        {{ parsedQuestion.x }} {{ parsedQuestion.op }} {{ parsedQuestion.y }}
      </div>
      <div v-else-if="isVertical && parsedQuestion" class="question-vertical">
        <div class="qv-operator">{{ parsedQuestion.op }}</div>
        <div class="qv-numbers">
          <div class="qv-x">{{ parsedQuestion.x }}</div>
          <div class="qv-y">{{ parsedQuestion.y }}</div>
        </div>
      </div>

      <div v-if="parsedQuestion?.isDiv" class="division-hint">
        输入: 商 余数 (shāng yúshù — quotient remainder)<br/>
        例如 (example): 11 ÷ 7 → <kbd>1 4</kbd>
      </div>

      <input
        ref="answerInput"
        class="answer-input"
        v-model="answer"
        :disabled="playerAnswered"
        placeholder="输入答案 (shūrù dá'àn — enter answer)"
        @keydown="onKeydown"
        autofocus
      />

      <button
        class="btn btn-primary"
        @click="submitAnswer"
        :disabled="!answer.trim() || playerAnswered"
      >
        提交 (tíjiāo — Submit)
      </button>

      <div
        v-if="result"
        class="result-text"
        :class="result === 'correct' ? 'result-correct' : 'result-wrong'"
      >
        玩家: {{ result === 'correct' ? '正确！ (zhèngquè — Correct!)' : '错误 (cuòwù — Wrong)' }}
      </div>

      <!-- AI States -->
      <div v-for="i in aiCount" :key="'ai-state-'+i" class="ai-section">
        <div v-if="aiRevealed[i-1]" class="ai-revealed">
          <div class="ai-result-row">
            <span class="ai-answer">🤖AI{{ i }}: <strong>{{ aiAnswers[i-1] }}</strong></span>
            <span class="ai-verdict" :class="aiCorrects[i-1] ? 'result-correct' : 'result-wrong'">
              ({{ aiCorrects[i-1] ? '正确' : '错误' }})
            </span>
          </div>
        </div>
        <div v-else-if="aiAnswered[i-1]" class="ai-thinking">🤖AI{{ i }} 已回答 (yǐ huídá — answered)</div>
        <div v-else-if="aiPhase[i-1] === 'reading'" class="ai-thinking">🤖AI{{ i }} 正在读题... (zhèngzài dú tí — reading...)</div>
        <div v-else-if="aiPhase[i-1] === 'thinking'" class="ai-thinking">🤖AI{{ i }} 思考中... (sīkǎo zhōng — thinking...)</div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.score-row {
  display: flex;
  flex-wrap: wrap;
  gap: 0.8rem;
  align-items: center;
  justify-content: space-between;
}
.ai-score { color: #48c6ef; font-size: 0.9rem; }
.question-vertical {
  display: flex; align-items: center; justify-content: center; gap: 1.5rem;
  margin: 1.5rem 0; font-family: 'Courier New', monospace;
}
.qv-operator { font-size: 2.5rem; font-weight: 700; color: #b0b0ff; }
.qv-numbers { display: flex; flex-direction: column; align-items: flex-end; font-size: 2.5rem; font-weight: 700; color: #fff; }
.qv-x, .qv-y { padding: 0.1rem 0; }
.division-hint { font-size: 0.85rem; color: #b0b0ff; margin-bottom: 0.8rem; line-height: 1.5; }
.division-hint kbd { background: rgba(108, 99, 255, 0.2); border: 1px solid rgba(108, 99, 255, 0.4); border-radius: 4px; padding: 0.1rem 0.4rem; font-family: 'Courier New', monospace; font-size: 0.9rem; }
.ai-section { margin-top: 0.5rem; padding: 0.4rem 0.5rem; border-radius: 8px; background: rgba(72, 198, 239, 0.08); border: 1px solid rgba(72, 198, 239, 0.15); }
.ai-thinking { color: #48c6ef; font-size: 0.85rem; animation: pulse 1.5s ease-in-out infinite; }
@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
.ai-revealed .ai-answer { color: #48c6ef; font-size: 0.9rem; }
.ai-revealed .ai-answer strong { font-family: 'Courier New', monospace; font-size: 1.1rem; }
.ai-result-row { display: flex; align-items: center; gap: 0.5rem; }
.ai-verdict { font-size: 0.85rem; font-weight: 600; }
</style>
