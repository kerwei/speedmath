<script setup>
import { ref, onMounted } from 'vue'

const props = defineProps({ sessionId: String })
const emit = defineEmits(['restart'])

const results = ref('')
const loading = ref(true)

onMounted(async () => {
  const params = new URLSearchParams({ session_id: props.sessionId })
  const res = await fetch(`/api/game/results?${params}`)
  const data = await res.json()
  results.value = data.results
  loading.value = false
})
</script>

<template>
  <div class="card">
    <h2>游戏结果 (yóuxì jiéguǒ — Game Results)</h2>

    <div v-if="loading">加载中 (jiāzài zhōng — Loading)...</div>

    <pre v-else class="results-pre">{{ results }}</pre>

    <button class="btn btn-primary" @click="emit('restart')" style="margin-top: 1.5rem">
      再来一局 (zài lái yī jú — Play Again)
    </button>
  </div>
</template>

<style scoped>
.results-pre {
  text-align: left;
  background: rgba(0, 0, 0, 0.3);
  padding: 1rem;
  border-radius: 8px;
  font-size: 1rem;
  line-height: 1.6;
  white-space: pre-wrap;
  margin: 1rem 0;
}
</style>
