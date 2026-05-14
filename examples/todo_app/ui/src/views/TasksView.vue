<script setup>
import { onMounted } from 'vue'
import TodoForm from '../components/TodoForm.vue'
import TodoFilters from '../components/TodoFilters.vue'
import TodoList from '../components/TodoList.vue'
import { useCategoriesStore } from '../store/categories.js'
import { useTodosStore } from '../store/todos.js'

const cats = useCategoriesStore()
const todos = useTodosStore()

onMounted(() => {
  if (todos.filterStatus === 'done') todos.filterStatus = ''
})

const emit = defineEmits(['edit-todo', 'delete-todo', 'clear-error', 'show-done', 'show-archive'])

function onTodoSubmit(payload, isEdit) {
  if (isEdit) return
  emit('clear-error')
  todos.createOne(payload)
}

function onEdit(t) {
  emit('edit-todo', { ...t })
}

function onDelete(id) {
  emit('delete-todo', id)
}
</script>

<template>
  <TodoForm :categories="cats.items" @submit="onTodoSubmit" />

  <section class="nv-card">
    <div class="tasks-head">
      <h2 class="h2">Tasks</h2>
      <div class="tasks-actions">
        <button type="button" class="nv-btn nv-btn-ghost" @click="emit('show-archive')">Archive</button>
        <button type="button" class="nv-btn nv-btn-primary" @click="emit('show-done')">Done</button>
      </div>
    </div>
    <TodoFilters v-model:status="todos.filterStatus" v-model:priority="todos.filterPriority" />
    <TodoList :todos="todos.filtered" :categories="cats.items" @edit="onEdit" @delete="onDelete" />
  </section>
</template>

<style scoped>
.tasks-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--nv-space-3);
  flex-wrap: wrap;
  margin-bottom: var(--nv-space-3);
}
.tasks-actions {
  display: flex;
  flex-wrap: wrap;
  gap: var(--nv-space-2);
  align-items: center;
}
.h2 {
  margin: 0;
  font-size: 1.1rem;
}
</style>
