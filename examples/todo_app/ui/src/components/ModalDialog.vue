<script setup>
import { onMounted, onUnmounted, ref, watch } from 'vue'

const props = defineProps({
  open: { type: Boolean, default: false },
  titleId: { type: String, default: 'nv-modal-title' },
})

const emit = defineEmits(['close'])

const panelRef = ref(null)

function onKeydown(e) {
  if (e.key === 'Escape' && props.open) {
    e.preventDefault()
    emit('close')
  }
}

function onBackdrop() {
  emit('close')
}

watch(
  () => props.open,
  (v) => {
    if (typeof document === 'undefined') return
    if (v) {
      document.body.style.overflow = 'hidden'
      requestAnimationFrame(() => {
        const root = panelRef.value
        if (!root) return
        const focusable = root.querySelector(
          'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])',
        )
        if (focusable && typeof focusable.focus === 'function') focusable.focus()
        else root.focus()
      })
    } else {
      document.body.style.overflow = ''
    }
  },
)

onMounted(() => {
  if (typeof document !== 'undefined') document.addEventListener('keydown', onKeydown)
})
onUnmounted(() => {
  if (typeof document !== 'undefined') {
    document.removeEventListener('keydown', onKeydown)
    document.body.style.overflow = ''
  }
})
</script>

<template>
  <Teleport to="body">
    <div v-if="open" class="nv-modal-root" role="presentation">
      <div class="nv-modal-backdrop" aria-hidden="true" @click="onBackdrop" />
      <div
        ref="panelRef"
        class="nv-modal-panel"
        role="dialog"
        aria-modal="true"
        :aria-labelledby="titleId"
        tabindex="-1"
        @click.stop
      >
        <slot />
      </div>
    </div>
  </Teleport>
</template>

<style scoped>
.nv-modal-root {
  position: fixed;
  inset: 0;
  z-index: 1000;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: var(--nv-space-4);
}
.nv-modal-backdrop {
  position: absolute;
  inset: 0;
  background: rgba(28, 37, 65, 0.45);
}
.nv-modal-panel {
  position: relative;
  z-index: 1;
  width: min(100%, 32rem);
  max-height: min(90vh, 40rem);
  overflow: auto;
  background: var(--nv-surface);
  border-radius: var(--nv-radius);
  box-shadow: var(--nv-shadow), 0 24px 80px rgba(28, 37, 65, 0.2);
  border: 1px solid var(--nv-border);
  outline: none;
}
</style>
