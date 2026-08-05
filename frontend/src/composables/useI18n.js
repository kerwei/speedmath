import { ref, computed } from 'vue'
import zh from '../locales/zh.json'
import en from '../locales/en.json'

const locales = { zh, en }
const currentLocale = ref(localStorage.getItem('locale') || 'zh')

export function useI18n() {
  function t(key, ...args) {
    let text = locales[currentLocale.value]?.[key]
    if (text === undefined) text = key
    if (args.length > 0) {
      args.forEach((arg, i) => {
        text = text.replace(`{${i}}`, arg)
      })
    }
    return text
  }

  function setLocale(lang) {
    if (locales[lang]) {
      currentLocale.value = lang
      localStorage.setItem('locale', lang)
    }
  }

  const locale = computed(() => currentLocale.value)
  const isZh = computed(() => currentLocale.value === 'zh')

  return { t, setLocale, locale, isZh }
}
