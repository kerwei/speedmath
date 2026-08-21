// 多人游戏 WebSocket (duōrén yóuxì WebSocket — multiplayer game WebSocket)
// Manages the WebSocket connection for real-time gameplay: questions, answers, leaderboard
import { ref } from 'vue'
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

// Lobby state — updated in real-time via WebSocket room_state messages
const lobbyState = ref({ players: [], code: '', roomId: 0, hostId: 0 })

let ws = null

export function useGameWs() {
  let roomId = 0

  function connect(room_id) {
    roomId = room_id
    _connect()
  }

  function _connect() {
    // 先关闭旧连接，避免连接泄漏 (xiān guānbì jiù liánjiē — close previous socket first to avoid leaks)
    if (ws) {
      ws.onclose = null
      try { ws.close() } catch (e) {}
      ws = null
    }
    const url = `${window.location.protocol === 'https:' ? 'wss' : 'ws'}://${window.location.host}/api/ws?token=${token.value}&room_id=${roomId}`
    const sock = new WebSocket(url)
    ws = sock

    sock.onopen = () => {
      console.log('[WS] connected, room=', roomId)
    }

    sock.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data)
        _handleMessage(data, roomId)
      } catch (err) {
        console.error('[WS] parse error:', err)
      }
    }

    sock.onclose = () => {
      console.log('[WS] disconnected')
      // 仅当仍指向本连接时才清空 (jǐn dāng réng zhǐxiàng běn liánjiē shí cái qīngkōng — only null if this is still the active socket)
      if (ws === sock) ws = null
    }

    sock.onerror = (err) => {
      console.error('[WS] error:', err)
    }
  }

  function _handleMessage(data, rid) {
    switch (data.type) {
      case 'room_state':
        lobbyState.value = {
          players: (data.players || []).map(p => ({
            userId: p.user_id,
            username: p.username.split('@')[0],
            ready: p.ready
          })),
          code: data.code || '',
          roomId: data.room_id || rid,
          hostId: data.host_id || 0
        }
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
      ws.onclose = null
      ws.close()
      ws = null
    }
  }

  return {
    gamePhase, question, qNum, total, players, answers, results, lobbyState,
    connect, disconnect, startGame, submitAnswer
  }
}
