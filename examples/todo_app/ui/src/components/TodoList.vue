<script setup>
import TodoItem from './TodoItem.vue'

defineProps({
  todos: { type: Array, default: () => [] },
  categories: { type: Array, default: () => [] },
})

const emit = defineEmits(['edit', 'delete'])
</script>

<template>
  <section class="list">
    <p v-if="!todos.length" class="empty">No todos match the current filters.</p>
    <ul v-else class="ul">
      <li v-for="t in todos" :key="t.id" class="li">
        <TodoItem :todo="t" :categories="categories" @edit="emit('edit', $event)" @delete="emit('delete', $event)" />
      </li>
    </ul>
  </section>
</template>

<style scoped>
.list {
  margin-top: var(--nv-space-2);
}
.ul {
  list-style: none;
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: var(--nv-space-3);
}
.empty {
  color: var(--nv-muted);
  margin: var(--nv-space-3) 0;
}
</style>
