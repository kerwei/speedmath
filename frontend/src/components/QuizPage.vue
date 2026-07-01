<script setup>
import { ref, computed, onMounted } from 'vue'

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

const progress = computed(() =>
  total.value > 0 ? (score.value / total.value) * 100 : 0
)

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
  await nextQuestion()
  loading.value = false
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
      <div class="question-text">{{ question }}</div>

      <input
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
