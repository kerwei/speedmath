<script setup>
import { ref } from 'vue'
import WelcomePage from './components/WelcomePage.vue'
import AuthPage from './components/AuthPage.vue'
import LobbyPage from './components/LobbyPage.vue'
import QuizPage from './components/QuizPage.vue'
import ResultsPage from './components/ResultsPage.vue'
import { useI18n } from './composables/useI18n.js'
import { useAuth } from './composables/useAuth.js'
import { useGameWs } from './composables/useGameWs.js'

const { t, setLocale, locale } = useI18n()
const { isLoggedIn } = useAuth()
const { gamePhase } = useGameWs()

const page = ref('welcome')
const sessionId = ref('')
const config = ref({})
const multiplayer = ref(false)

function onStart(cfg) {
  config.value = cfg
  page.value = 'quiz'
}

function onMultiplayer() {
  page.value = isLoggedIn() ? 'lobby' : 'auth'
}

function onLobbyStart() {
  multiplayer.value = true
  config.value = {}  // multiplayer config comes via WS
  page.value = 'quiz'
}

function onAuthDone() {
  page.value = 'lobby'
}

function onFinish(sid) {
  sessionId.value = sid
  multiplayer.value = false
  page.value = 'results'
}

function onRestart() {
  sessionId.value = ''
  config.value = {}
  multiplayer.value = false
  page.value = 'welcome'
}

function onBackFromLobby() {
  multiplayer.value = false
  page.value = 'welcome'
}

function toggleLang() {
  setLocale(locale.value === 'zh' ? 'en' : 'zh')
}
</script>

<template>
  <div class="app-container">
    <div class="app-header">
      <div class="app-title-block">
        <h1 class="app-title">速算</h1>
        <span class="app-subtitle">speedmath</span>
      </div>
      <button class="lang-toggle" @click="toggleLang">{{ locale === 'zh' ? 'EN' : '中' }}</button>
    </div>
    <WelcomePage v-if="page === 'welcome'" @start="onStart" @multiplayer="onMultiplayer" />
    <AuthPage v-else-if="page === 'auth'" @done="onAuthDone" />
    <LobbyPage v-else-if="page === 'lobby'" @start="onLobbyStart" @back="onBackFromLobby" />
    <QuizPage
      v-else-if="page === 'quiz'"
      :config="config"
      :multiplayer="multiplayer"
      @finish="onFinish"
    />
    <ResultsPage
      v-else-if="page === 'results'"
      :session-id="sessionId"
      @restart="onRestart"
    />
  </div>
</template>

<style scoped>
.app-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  margin-bottom: 2rem;
}

.app-title-block {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.app-title {
  margin: 0;
  font-size: 2.8rem;
  letter-spacing: 0.25em;
}

.app-subtitle {
  font-size: 0.8rem;
  color: #888;
  letter-spacing: 0.15em;
  text-transform: lowercase;
}

.lang-toggle {
  background: rgba(108, 99, 255, 0.15);
  border: 1px solid rgba(108, 99, 255, 0.3);
  color: #b0b0ff;
  padding: 0.3rem 0.8rem;
  border-radius: 6px;
  cursor: pointer;
  font-size: 0.85rem;
  font-weight: 600;
  transition: background 0.2s;
}

.lang-toggle:hover {
  background: rgba(108, 99, 255, 0.3);
}
</style>
