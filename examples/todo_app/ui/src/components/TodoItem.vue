<script setup>
import { computed } from 'vue'
import CategoryBadge from './CategoryBadge.vue'
import { useTodosStore } from '../store/todos.js'
import { formatDueDateLabel } from '../utils/dueDate.js'

const props = defineProps({
  todo: { type: Object, required: true },
  categories: { type: Array, default: () => [] },
})

const emit = defineEmits(['edit', 'delete'])

const todos = useTodosStore()

const cat = computed(() => {
  const id = props.todo.categoryId
  if (id == null) return null
  return props.categories.find((c) => c.id === id) || null
})

const dueLabel = computed(() => formatDueDateLabel(props.todo.dueDate))

const showMarkDone = computed(() => {
  const s = props.todo.status
  return s !== 'done' && s !== 'archived' && s !== 'cancelled'
})

function onMarkDone() {
  todos.markDone(props.todo)
}
</script>

<template>
  <article class="item">
    <div class="main">
      <div class="top">
        <h3 class="t-title">{{ todo.title }}</h3>
        <CategoryBadge v-if="cat" :name="cat.name" :color="cat.color" />
      </div>
      <p v-if="todo.description" class="desc">{{ todo.description }}</p>
      <div class="meta">
        <span class="pill">{{ todo.status }}</span>
        <span class="pill pri">{{ todo.priority }}</span>
        <span v-if="dueLabel" class="pill due">{{ dueLabel }}</span>
      </div>
    </div>
    <div class="actions">
      <button
        v-if="showMarkDone"
        type="button"
        class="nv-btn nv-btn-done"
        @click="onMarkDone"
      >
        Done
      </button>
      <button type="button" class="nv-btn nv-btn-ghost" @click="emit('edit', todo)">Edit</button>
      <button type="button" class="nv-btn nv-btn-danger" @click="emit('delete', todo.id)">Delete</button>
    </div>
  </article>
</template>

<style scoped>
.item {
  display: flex;
  gap: var(--nv-space-3);
  justify-content: space-between;
  align-items: flex-start;
  padding: var(--nv-space-3);
  border: 1px solid var(--nv-border);
  border-radius: var(--nv-radius-sm);
  background: var(--nv-surface-2);
}
.main {
  flex: 1;
  min-width: 0;
}
.top {
  display: flex;
  flex-wrap: wrap;
  gap: var(--nv-space-2);
  align-items: center;
}
.t-title {
  margin: 0;
  font-size: 1.05rem;
}
.desc {
  margin: var(--nv-space-2) 0 0;
  color: var(--nv-muted);
  font-size: 0.9rem;
}
.meta {
  display: flex;
  gap: var(--nv-space-2);
  margin-top: var(--nv-space-2);
}
.pill {
  font-size: 0.72rem;
  font-weight: 700;
  padding: 0.12rem 0.45rem;
  border-radius: 6px;
  background: #e0e7ff;
  color: var(--nv-accent);
  text-transform: uppercase;
}
.pill.pri {
  background: #fef3c7;
  color: var(--nv-warning);
}
.pill.due {
  background: #ecfdf5;
  color: var(--nv-success);
  text-transform: none;
  font-weight: 600;
}
.actions {
  display: flex;
  flex-direction: column;
  gap: var(--nv-space-2);
}
.nv-btn-done {
  background: #ecfdf5;
  color: var(--nv-success);
  border: 1px solid rgba(16, 185, 129, 0.35);
  font-weight: 600;
}
.nv-btn-done:hover {
  background: #d1fae5;
}
</style>
