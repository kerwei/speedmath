<script setup>
import { ref } from 'vue'
import WelcomePage from './components/WelcomePage.vue'
import QuizPage from './components/QuizPage.vue'
import ResultsPage from './components/ResultsPage.vue'

const page = ref('welcome')
const sessionId = ref('')
const config = ref({})

function onStart(cfg) {
  config.value = cfg
  page.value = 'quiz'
}

function onFinish(sid) {
  sessionId.value = sid
  page.value = 'results'
}

function onRestart() {
  sessionId.value = ''
  config.value = {}
  page.value = 'welcome'
}
</script>

<template>
  <div class="app-container">
    <h1 class="app-title">🧮 SpeedMath 速算 (sùsuàn)</h1>
    <WelcomePage v-if="page === 'welcome'" @start="onStart" />
    <QuizPage
      v-else-if="page === 'quiz'"
      :config="config"
      @finish="onFinish"
    />
    <ResultsPage
      v-else-if="page === 'results'"
      :session-id="sessionId"
      @restart="onRestart"
    />
  </div>
</template>
