// 多人游戏 WebSocket (duōrén yóuxì WebSocket — multiplayer game WebSocket)
// Manages the WebSocket connection for real-time gameplay: questions, answers, leaderboard
import { ref, onUnmounted } from 'vue'
import { useAuth } from './useAuth.js'

const { token } = useAuth()

// Game state (shared across components)
const gamePhase = ref('idle')   // idle | playing | evaluating | finished
const question = ref('')
const qNum = ref(0)
const total = ref(0)
const players = ref([])         // {user_id, username, correct, time_ms, answered}
const answers = ref([])          // {user_id, answer} revealed during evaluation
const results = ref([])          // final results

let ws = null
let reconnectTimer = null

export function useGameWs() {
  let roomId = 0

  function connect(room_id) {
    roomId = room_id
    _connect()
  }

  function _connect() {
    const url = `${window.location.protocol === 'https:' ? 'wss' : 'ws'}://${window.location.host}/api/ws?token=${token.value}&room_id=${roomId}`
    ws = new WebSocket(url)

    ws.onopen = () => {
      console.log('[WS] connected, room=', roomId)
    }

    ws.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data)
        _handleMessage(data)
      } catch (err) {
        console.error('[WS] parse error:', err)
      }
    }

    ws.onclose = () => {
      console.log('[WS] disconnected')
      ws = null
    }

    ws.onerror = (err) => {
      console.error('[WS] error:', err)
    }
  }

  function _handleMessage(data) {
    switch (data.type) {
      case 'room_state':
        // Lobby state — handled by LobbyPage's own polling
        break

      case 'game_start':
        gamePhase.value = 'playing'
        players.value = (data.players || []).map(p => ({
          ...p,
          correct: 0,
          time_ms: 0,
          answered: false
        }))
        break

      case 'question':
        question.value = data.question
        qNum.value = data.q_num
        total.value = data.total
        gamePhase.value = 'playing'
        break

      case 'leaderboard':
        gamePhase.value = data.phase || 'evaluation'
        if (data.players) {
          players.value = data.players.map(p => ({
            ...p,
            answered: true
          }))
        }
        if (data.answers) {
          answers.value = data.answers
        }
        break

      case 'game_over':
        gamePhase.value = 'finished'
        results.value = data.players || []
        break
    }
  }

  function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj))
    }
  }

  function startGame(diff, intense, ops) {
    send({ type: 'start', diff, intense, ops })
  }

  function submitAnswer(answer) {
    send({ type: 'answer', answer: String(answer) })
  }

  function disconnect() {
    if (ws) {
      ws.close()
      ws = null
    }
    if (reconnectTimer) {
      clearTimeout(reconnectTimer)
      reconnectTimer = null
    }
  }

  return {
    gamePhase, question, qNum, total, players, answers, results,
    connect, disconnect, startGame, submitAnswer
  }
}
