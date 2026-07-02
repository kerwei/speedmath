<script setup>
import { ref, computed, onMounted, nextTick } from 'vue'

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
    ops: props.config.ops
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
  // 自动聚焦到输入框 (zìdòng jùjiāo dào shūrù kuàng — auto-focus input)
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
  // 自动进入下一题 (zìdòng jìnrù xià yī tí — auto-advance)
  setTimeout(() => nextQuestion(), 600)
}

function onKeydown(e) {
  if (e.key === 'Enter') {
    submitAnswer()
  }
}

onMounted(() => {
  newGame()
})
</script>

<template>
  <div class="card">
    <div class="score-text">
      得分 (défēn — Score): {{ score }} / {{ total }}
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
        例如 (example): 11 ÷ 7 → <kbd>1 4</kbd> 或 (or) <kbd>14</kbd>
      </div>

      <input
        ref="answerInput"
        class="answer-input"
        v-model="answer"
        placeholder="输入答案 (shūrù dá'àn — enter answer)"
        @keydown="onKeydown"
        autofocus
      />

      <button class="btn btn-primary" @click="submitAnswer" :disabled="!answer.trim()">
        提交 (tíjiāo — Submit)
      </button>

      <div
        v-if="result"
        class="result-text"
        :class="result === 'correct' ? 'result-correct' : 'result-wrong'"
      >
        {{ result === 'correct' ? '正确！ (zhèngquè — Correct!)' : '错误 (cuòwù — Wrong)' }}
      </div>
    </div>
  </div>
</template>

<style scoped>
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
</style>
