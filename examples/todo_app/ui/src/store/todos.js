import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import { nvSendTodo } from '../bridge.js'

export const useTodosStore = defineStore('todos', () => {
  const items = ref([])
  const error = ref('')
  const filterStatus = ref('')
  const filterPriority = ref('')

  const filtered = computed(() => {
    return items.value.filter((t) => {
      if (t.status === 'archived' || t.status === 'done') return false
      if (filterStatus.value && t.status !== filterStatus.value) return false
      if (filterPriority.value && t.priority !== filterPriority.value) return false
      return true
    })
  })

  const doneTodos = computed(() => items.value.filter((t) => t.status === 'done'))

  const archivedTodos = computed(() => items.value.filter((t) => t.status === 'archived'))

  function applyList(data) {
    items.value = Array.isArray(data) ? data : []
  }

  function fetchAll() {
    error.value = ''
    nvSendTodo({ action: 'todos.list' })
  }

  function createOne(payload) {
    error.value = ''
    nvSendTodo({ action: 'todos.create', payload })
  }

  function updateOne(payload) {
    error.value = ''
    nvSendTodo({ action: 'todos.update', payload })
  }

  function remove(id) {
    error.value = ''
    nvSendTodo({ action: 'todos.delete', payload: { id } })
  }

  function markDone(t) {
    if (!t || t.status === 'done' || t.status === 'archived' || t.status === 'cancelled') return
    error.value = ''
    nvSendTodo({
      action: 'todos.update',
      payload: {
        id: t.id,
        title: t.title || '',
        description: t.description || '',
        status: 'done',
        priority: t.priority || 'medium',
        categoryId: t.categoryId ?? 0,
        parentId: t.parentId ?? 0,
        dueDate: t.dueDate ?? 0,
        position: t.position ?? 0,
      },
    })
  }

  function archiveDoneIds(ids) {
    const idSet = new Set(Array.isArray(ids) ? ids : [])
    for (const id of idSet) {
      const t = items.value.find((x) => x.id === id)
      if (!t || t.status !== 'done') continue
      error.value = ''
      nvSendTodo({
        action: 'todos.update',
        payload: {
          id: t.id,
          title: t.title || '',
          description: t.description || '',
          status: 'archived',
          priority: t.priority || 'medium',
          categoryId: t.categoryId ?? 0,
          parentId: t.parentId ?? 0,
          dueDate: t.dueDate ?? 0,
          position: t.position ?? 0,
        },
      })
    }
  }

  function restoreArchivedIds(ids) {
    const idSet = new Set(Array.isArray(ids) ? ids : [])
    for (const id of idSet) {
      const t = items.value.find((x) => x.id === id)
      if (!t || t.status !== 'archived') continue
      error.value = ''
      nvSendTodo({
        action: 'todos.update',
        payload: {
          id: t.id,
          title: t.title || '',
          description: t.description || '',
          status: 'done',
          priority: t.priority || 'medium',
          categoryId: t.categoryId ?? 0,
          parentId: t.parentId ?? 0,
          dueDate: t.dueDate ?? 0,
          position: t.position ?? 0,
        },
      })
    }
  }

  return {
    items,
    error,
    filterStatus,
    filterPriority,
    filtered,
    doneTodos,
    archivedTodos,
    applyList,
    fetchAll,
    createOne,
    updateOne,
    remove,
    markDone,
    archiveDoneIds,
    restoreArchivedIds,
  }
})
