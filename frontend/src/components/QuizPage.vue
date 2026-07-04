<script setup>
import { ref, computed, onMounted, onUnmounted, nextTick } from 'vue'

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

// AI 对手状态 (AI duìshǒu zhuàngtài — AI opponent state)
const hasAi = computed(() => props.config.ai_level > 0)
const aiAnswer = ref('')
const aiDelay = ref(0)
const aiScore = ref(0)
const aiCorrect = ref(false)
const aiFinished = ref(false)
const aiAnswered = ref(false)
const aiRevealed = ref(false)
const aiPhase = ref('')  // '', 'reading', 'thinking'
const playerAnswered = ref(false)
const advancing = ref(false)
let aiTimer = null
let aiPhaseTimer = null

const aiResultText = computed(() => aiCorrect.value ? '正确' : '错误')
const aiResultClass = computed(() => aiCorrect.value ? 'result-correct' : 'result-wrong')

const progress = computed(() =>
  total.value > 0 ? (score.value / total.value) * 100 : 0
)

// 解析 "x op y" → { x, op, y }
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

// 计算 AI 正确性 (jìsuàn AI zhèngquèxìng — compute AI correctness)
function computeAiCorrect() {
  const p = parsedQuestion.value
  if (!p) return false
  const x = parseInt(p.x)
  const y = parseInt(p.y)
  if (p.isDiv) {
    const parts = aiAnswer.value.split(' ')
    if (parts.length === 2) {
      const q = parseInt(parts[0])
      const r = parseInt(parts[1])
      return (q * y + r === x)
    }
    return false
  } else if (p.op === '+') {
    return parseInt(aiAnswer.value) === x + y
  } else if (p.op === '-') {
    return parseInt(aiAnswer.value) === x - y
  } else if (p.op === '*') {
    return parseInt(aiAnswer.value) === x * y
  }
  return false
}

// 尝试推进到下一题 (chángshì tuījìn dào xià yī tí — try to advance)
function tryAdvance() {
  if (advancing.value) return
  if (playerAnswered.value && aiFinished.value) {
    advancing.value = true
    aiRevealed.value = true
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

async function newGame() {
  loading.value = true
  const data = await apiCall('/api/game/new', {
    diff: props.config.diff,
    intense: props.config.intense,
    ops: props.config.ops,
    ai_level: props.config.ai_level || 0
  })
  sessionId.value = data.session_id
  total.value = props.config.intense * 10
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
  aiFinished.value = false
  aiAnswered.value = false
  aiRevealed.value = false
  advancing.value = false
  aiCorrect.value = false
  aiPhase.value = ''
  if (aiTimer) clearTimeout(aiTimer)
  if (aiPhaseTimer) clearTimeout(aiPhaseTimer)
  aiTimer = null
  aiPhaseTimer = null

  // Store AI info and start AI timer immediately
  if (hasAi.value && data.ai_answer) {
    aiAnswer.value = data.ai_answer
    aiDelay.value = parseInt(data.ai_delay_ms) || 2000

    // Phase 1: reading (0 - 700ms)
    aiPhase.value = 'reading'
    aiPhaseTimer = setTimeout(() => {
      // Phase 2: thinking (700ms - ai_delay)
      aiPhase.value = 'thinking'
    }, 700)

    // AI finishes computing at ai_delay
    aiTimer = setTimeout(() => {
      if (computeAiCorrect()) aiScore.value++
      aiCorrect.value = computeAiCorrect()
      aiFinished.value = true
      aiAnswered.value = true
      tryAdvance()
    }, aiDelay.value)
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
  if (e.key === 'Enter') {
    submitAnswer()
  }
}

onMounted(() => {
  newGame()
})

onUnmounted(() => {
  if (aiTimer) clearTimeout(aiTimer)
  if (aiPhaseTimer) clearTimeout(aiPhaseTimer)
})
</script>

<template>
  <div class="card">
    <div class="score-row">
      <div class="score-text">
        😎 {{ score }} / {{ total }}
      </div>
      <div v-if="hasAi" class="score-text ai-score">
        🤖 {{ aiScore }} / {{ total }}
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
      <!-- 水平显示 (shuǐpíng xiǎnshì — horizontal display) -->
      <div v-if="!isVertical && parsedQuestion" class="question-text">
        {{ parsedQuestion.x }} {{ parsedQuestion.op }} {{ parsedQuestion.y }}
      </div>

      <!-- 垂直显示 (chuízhí xiǎnshì — vertical display) -->
      <div v-else-if="isVertical && parsedQuestion" class="question-vertical">
        <div class="qv-operator">{{ parsedQuestion.op }}</div>
        <div class="qv-numbers">
          <div class="qv-x">{{ parsedQuestion.x }}</div>
          <div class="qv-y">{{ parsedQuestion.y }}</div>
        </div>
      </div>

      <!-- 除法提示 (chúfǎ tíshì — division hint) -->
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

      <!-- 玩家结果 (wánjiā jiéguǒ — player result) -->
      <div
        v-if="result"
        class="result-text"
        :class="result === 'correct' ? 'result-correct' : 'result-wrong'"
      >
        玩家: {{ result === 'correct' ? '正确！ (zhèngquè — Correct!)' : '错误 (cuòwù — Wrong)' }}
      </div>

      <!-- AI 结果 (AI jiéguǒ — AI result) -->
      <div v-if="hasAi" class="ai-section">
        <div v-if="aiRevealed" class="ai-revealed">
          <div class="ai-result-row">
            <span class="ai-answer">🤖 AI: <strong>{{ aiAnswer }}</strong></span>
            <span class="ai-verdict" :class="aiResultClass">({{ aiResultText }})</span>
          </div>
        </div>
        <div v-else-if="aiAnswered" class="ai-thinking">
          🤖 AI 已回答 (yǐ huídá — answered)
        </div>
        <div v-else-if="aiPhase === 'reading'" class="ai-thinking">
          🤖 AI 正在读题... (zhèngzài dú tí — reading question...)
        </div>
        <div v-else-if="aiPhase === 'thinking'" class="ai-thinking">
          🤖 AI 思考中... (sīkǎo zhōng — thinking...)
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.score-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.ai-score {
  color: #48c6ef;
}

.question-vertical {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 1.5rem;
  margin: 1.5rem 0;
  font-family: 'Courier New', monospace;
}

.qv-operator {
  font-size: 2.5rem;
  font-weight: 700;
  color: #b0b0ff;
}

.qv-numbers {
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  font-size: 2.5rem;
  font-weight: 700;
  color: #fff;
}

.qv-x, .qv-y {
  padding: 0.1rem 0;
}

.division-hint {
  font-size: 0.85rem;
  color: #b0b0ff;
  margin-bottom: 0.8rem;
  line-height: 1.5;
}

.division-hint kbd {
  background: rgba(108, 99, 255, 0.2);
  border: 1px solid rgba(108, 99, 255, 0.4);
  border-radius: 4px;
  padding: 0.1rem 0.4rem;
  font-family: 'Courier New', monospace;
  font-size: 0.9rem;
}

.ai-section {
  margin-top: 1rem;
  padding: 0.5rem;
  border-radius: 8px;
  background: rgba(72, 198, 239, 0.08);
  border: 1px solid rgba(72, 198, 239, 0.15);
}

.ai-thinking {
  color: #48c6ef;
  font-size: 0.95rem;
  animation: pulse 1.5s ease-in-out infinite;
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.4; }
}

.ai-revealed .ai-answer {
  color: #48c6ef;
  font-size: 1rem;
}

.ai-revealed .ai-answer strong {
  font-family: 'Courier New', monospace;
  font-size: 1.2rem;
}

.ai-result-row {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.ai-verdict {
  font-size: 0.9rem;
  font-weight: 600;
}
</style>
