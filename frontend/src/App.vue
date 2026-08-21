<script setup>
import { ref } from 'vue'
import WelcomePage from './components/WelcomePage.vue'
import AuthPage from './components/AuthPage.vue'
import LobbyPage from './components/LobbyPage.vue'
import ProfilePage from './components/ProfilePage.vue'
import QuizPageSolo from './components/QuizPageSolo.vue'
import QuizPageMulti from './components/QuizPageMulti.vue'
import ResultsPage from './components/ResultsPage.vue'
import { useI18n } from './composables/useI18n.js'
import { useAuth } from './composables/useAuth.js'
import { useGameWs } from './composables/useGameWs.js'

const { t, setLocale, locale } = useI18n()
const { isLoggedIn } = useAuth()
const { gamePhase, disconnect } = useGameWs()

const page = ref('welcome')
const sessionId = ref('')
const config = ref({})
const multiplayer = ref(false)
const mpResults = ref([])
// 登录后跳转目标 (dēnglù hòu tiàozhuǎn mùbiāo — page to return to after auth)
const redirectAfterAuth = ref('welcome')

function onStart(cfg) {
  config.value = cfg
  page.value = 'quiz'
}

// 前往登录页并记录来源页面 (qiánwǎng dēnglù yè bìng jìlù láiyuán — go to auth, remember where to return)
function goAuth(intent) {
  redirectAfterAuth.value = intent
  page.value = 'auth'
}

function onMultiplayer() {
  // 未登录时先登录, 登录后回到多人游戏 (dēnglù hòu huídào — login first, return to lobby after)
  if (isLoggedIn()) page.value = 'lobby'
  else goAuth('lobby')
}

function onLobbyStart() {
  multiplayer.value = true
  config.value = {}
  page.value = 'quiz'
}

function onAuthDone() {
  // 返回用户点击登录按钮前所在的页面 (fǎnhuí yuánlái yèmiàn — return to the page the user came from)
  page.value = redirectAfterAuth.value
}

function onFinish(detail) {
  if (detail && detail.multiplayer) {
    mpResults.value = detail.players || []
    page.value = 'results'
  } else {
    sessionId.value = detail
    page.value = 'results'
  }
  multiplayer.value = false
}

function onRestart() {
  // 返回首页时确保 WebSocket 已关闭 (fǎnhuí shǒuyè shí — ensure WS is closed when returning to home)
  disconnect()
  sessionId.value = ''
  config.value = {}
  mpResults.value = []
  multiplayer.value = false
  page.value = 'welcome'
}

function onBackFromLobby() {
  multiplayer.value = false
  page.value = 'welcome'
}

function onProfile() {
  // 始终打开个人资料页; 未登录时页面内提供登录入口 (lóugòu — open profile; page handles login itself)
  page.value = 'profile'
}

function onProfileLogin() {
  goAuth('profile')
}

function onBackFromProfile() {
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
        <span class="app-subtitle">quickmath</span>
      </div>
      <button class="lang-toggle" @click="toggleLang">{{ locale === 'zh' ? 'EN' : '中' }}</button>
    </div>
    <WelcomePage v-if="page === 'welcome'" @start="onStart" @multiplayer="onMultiplayer" @profile="onProfile" />
    <AuthPage v-else-if="page === 'auth'" @done="onAuthDone" />
    <LobbyPage v-else-if="page === 'lobby'" @start="onLobbyStart" @back="onBackFromLobby" />
    <ProfilePage v-else-if="page === 'profile'" @back="onBackFromProfile" @login="onProfileLogin" />
    <QuizPageSolo
      v-else-if="page === 'quiz' && !multiplayer"
      :config="config"
      @finish="onFinish"
    />
    <QuizPageMulti
      v-else-if="page === 'quiz' && multiplayer"
      :config="config"
      @finish="onFinish"
    />
    <ResultsPage
      v-else-if="page === 'results'"
      :session-id="sessionId"
      :mp-players="mpResults"
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
