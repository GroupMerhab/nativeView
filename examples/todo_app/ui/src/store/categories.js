import { defineStore } from 'pinia'
import { ref } from 'vue'
import { nvSendTodo } from '../bridge.js'

export const useCategoriesStore = defineStore('categories', () => {
  const items = ref([])
  const error = ref('')

  function applyList(data) {
    items.value = Array.isArray(data) ? data : []
  }

  function fetchAll() {
    error.value = ''
    nvSendTodo({ action: 'categories.list' })
  }

  function createOne(name, color) {
    error.value = ''
    nvSendTodo({ action: 'categories.create', payload: { name, color } })
  }

  function remove(id) {
    error.value = ''
    nvSendTodo({ action: 'categories.delete', payload: { id } })
  }

  return { items, error, applyList, fetchAll, createOne, remove }
})
