<script setup>
defineProps({
  modelValue: { type: String, required: true },
})

const emit = defineEmits(['update:modelValue'])

const items = [
  { id: 'tasks', label: 'Tasks', icon: '☰' },
  { id: 'done', label: 'Done', icon: '✓' },
  { id: 'archive', label: 'Archive', icon: '▣' },
  { id: 'categories', label: 'Categories', icon: '◧' },
]
</script>

<template>
  <nav class="nv-side" aria-label="Main">
    <button
      v-for="it in items"
      :key="it.id"
      type="button"
      class="nv-side-btn"
      :class="{ active: modelValue === it.id }"
      :aria-current="modelValue === it.id ? 'page' : undefined"
      @click="emit('update:modelValue', it.id)"
    >
      <span class="ico" aria-hidden="true">{{ it.icon }}</span>
      <span class="lbl">{{ it.label }}</span>
    </button>
  </nav>
</template>

<style scoped>
.nv-side {
  display: flex;
  flex-direction: column;
  gap: var(--nv-space-2);
  padding: var(--nv-space-3) var(--nv-space-2);
  background: var(--nv-surface);
  border-right: 1px solid var(--nv-border);
  min-height: 100vh;
  width: 6.75rem;
  flex-shrink: 0;
}
.nv-side-btn {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: var(--nv-space-1);
  padding: var(--nv-space-2) var(--nv-space-1);
  border: 1px solid transparent;
  border-radius: var(--nv-radius-sm);
  background: transparent;
  color: var(--nv-muted);
  font: inherit;
  font-size: 0.65rem;
  font-weight: 600;
  cursor: pointer;
  text-align: center;
  line-height: 1.15;
}
.nv-side-btn:hover {
  background: var(--nv-surface-2);
  color: var(--nv-text);
}
.nv-side-btn.active {
  border-color: var(--nv-border);
  background: linear-gradient(145deg, rgba(99, 102, 241, 0.12), rgba(139, 92, 246, 0.08));
  color: var(--nv-accent);
}
.ico {
  font-size: 1.25rem;
  line-height: 1;
}
.lbl {
  max-width: 100%;
  word-break: break-word;
}
</style>
