<script setup>
import { ref } from 'vue'

const emit = defineEmits(['start'])

const diff = ref(1)
const intense = ref(1)
const ops = ref({ '1': true, '2': true, '3': true, '4': true })

function startGame() {
  const selected = Object.entries(ops.value)
    .filter(([, v]) => v)
    .map(([k]) => k)
    .join('')

  emit('start', { diff: diff.value, intense: intense.value, ops: selected })
}
</script>

<template>
  <div class="card">
    <h2>游戏设置 (yóuxì shèzhì — Game Settings)</h2>

    <div class="form-group">
      <label>难度 (nándù — Difficulty)</label>
      <select v-model.number="diff">
        <option :value="1">1位 (1-digit)</option>
        <option :value="2">2位 (2-digit)</option>
        <option :value="3">3位 (3-digit)</option>
      </select>
    </div>

    <div class="form-group">
      <label>题量 (tíliàng — Question Count)</label>
      <select v-model.number="intense">
        <option :value="1">10 题 (questions)</option>
        <option :value="2">20 题</option>
        <option :value="3">30 题</option>
      </select>
    </div>

    <div class="form-group">
      <label>运算符 (yùnsuànfú — Operators)</label>
      <div class="ops-group">
        <label><input type="checkbox" value="1" v-model="ops['1']" /> + 加 (jiā)</label>
        <label><input type="checkbox" value="2" v-model="ops['2']" /> - 减 (jiǎn)</label>
        <label><input type="checkbox" value="3" v-model="ops['3']" /> × 乘 (chéng)</label>
        <label><input type="checkbox" value="4" v-model="ops['4']" /> ÷ 除 (chú)</label>
      </div>
    </div>

    <button class="btn btn-primary" @click="startGame">开始 (kāishǐ — Start)</button>
  </div>
</template>
