<script setup>
import { ref, computed } from 'vue'
import { useI18n } from '../composables/useI18n.js'

const { t } = useI18n()
const emit = defineEmits(['start'])

const diff = ref(1)
const intense = ref(1)
const vDisplay = ref('horizontal')
const aiCount = ref(1)
const aiDiff = ref([1, 2, 2])
const ops = ref({ '1': true, '2': true, '3': true, '4': true })

const visibleAiDiffs = computed(() => aiDiff.value.slice(0, aiCount.value))

function startGame() {
  const selected = Object.entries(ops.value)
    .filter(([, v]) => v)
    .map(([k]) => k)
    .join('')
  const levels = visibleAiDiffs.value.filter(v => v > 0).join(',')
  emit('start', {
    diff: diff.value,
    intense: intense.value,
    ops: selected,
    vDisplay: vDisplay.value,
    ai_levels: levels
  })
}
</script>

<template>
  <div class="card">
    <h2>{{ t('settings.title') }}</h2>

    <div class="form-group">
      <label>{{ t('settings.difficulty') }}</label>
      <select v-model.number="diff">
        <option :value="1">{{ t('settings.digit.1') }}</option>
        <option :value="2">{{ t('settings.digit.2') }}</option>
        <option :value="3">{{ t('settings.digit.3') }}</option>
      </select>
    </div>

    <div class="form-group">
      <label>{{ t('settings.questions') }}</label>
      <select v-model.number="intense">
        <option :value="1">{{ t('settings.questions.10') }}</option>
        <option :value="2">{{ t('settings.questions.20') }}</option>
        <option :value="3">{{ t('settings.questions.30') }}</option>
      </select>
    </div>

    <div class="form-group">
      <label>{{ t('settings.layout') }}</label>
      <div class="display-group">
        <label>
          <input type="radio" v-model="vDisplay" value="horizontal" />
          {{ t('settings.layout.horizontal') }} — {{ t('settings.layout.hint.horizontal') }}
        </label>
        <label>
          <input type="radio" v-model="vDisplay" value="vertical" />
          {{ t('settings.layout.vertical') }} — <span class="stacked-preview">+ <span>3<br/>5</span></span>
        </label>
      </div>
    </div>

    <div class="form-group">
      <label>{{ t('settings.ai.count') }}</label>
      <select v-model.number="aiCount">
        <option :value="0">{{ t('settings.ai.none') }}</option>
        <option :value="1">1</option>
        <option :value="2">2</option>
        <option :value="3">3</option>
      </select>
    </div>

    <div class="form-group" v-for="(_, idx) in aiCount" :key="idx">
      <label>{{ t('ai.prefix') }}{{ idx + 1 }} {{ t('settings.ai.difficulty') }}</label>
      <select v-model.number="aiDiff[idx]">
        <option value="1">{{ t('settings.ai.easy') }} — {{ t('settings.ai.easy.hint') }}</option>
        <option value="2">{{ t('settings.ai.medium') }} — {{ t('settings.ai.medium.hint') }}</option>
        <option value="3">{{ t('settings.ai.hard') }} — {{ t('settings.ai.hard.hint') }}</option>
      </select>
    </div>

    <div class="form-group">
      <label>{{ t('settings.operators') }}</label>
      <div class="ops-group">
        <label><input type="checkbox" value="1" v-model="ops['1']" /> + {{ t('settings.op.add') }}</label>
        <label><input type="checkbox" value="2" v-model="ops['2']" /> - {{ t('settings.op.sub') }}</label>
        <label><input type="checkbox" value="3" v-model="ops['3']" /> × {{ t('settings.op.mul') }}</label>
        <label><input type="checkbox" value="4" v-model="ops['4']" /> ÷ {{ t('settings.op.div') }}</label>
      </div>
    </div>

    <button class="btn btn-primary" @click="startGame">{{ t('settings.start') }}</button>
  </div>
</template>

<style scoped>
.display-group { display: flex; flex-direction: column; gap: 0.5rem; }
.display-group label { display: flex; align-items: center; gap: 0.4rem; cursor: pointer; font-size: 0.95rem; color: #b0b0ff; }
.display-group input[type="radio"] { width: auto; accent-color: #6c63ff; }
.stacked-preview { display: inline-flex; align-items: center; gap: 0.3rem; font-family: 'Courier New', monospace; font-size: 1rem; line-height: 1.2; vertical-align: middle; }
.stacked-preview > span { display: inline-flex; flex-direction: column; align-items: center; }
</style>
