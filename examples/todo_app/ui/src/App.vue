<script setup>
import { onMounted, ref } from 'vue'
import { useCategoriesStore } from './store/categories.js'
import { useTodosStore } from './store/todos.js'
import { nvOn } from './bridge.js'
import ModalDialog from './components/ModalDialog.vue'
import SideToolbar from './components/SideToolbar.vue'
import TodoEditModal from './components/TodoEditModal.vue'
import ArchiveView from './views/ArchiveView.vue'
import CategoriesView from './views/CategoriesView.vue'
import DoneView from './views/DoneView.vue'
import TasksView from './views/TasksView.vue'

const cats = useCategoriesStore()
const todos = useTodosStore()
const activeView = ref('tasks')
const editingTodo = ref(null)
const editModalOpen = ref(false)
const deleteConfirmOpen = ref(false)
const pendingDeleteId = ref(null)
const pendingDeleteTitle = ref('')
const bridgeErr = ref('')
const deleteConfirmTitleId = 'todo-delete-confirm-title'

onMounted(() => {
  nvOn('todos.list', (d) => {
    todos.applyList(d)
  })
  nvOn('categories.list', (d) => {
    cats.applyList(d)
  })
  nvOn('error', (d) => {
    bridgeErr.value = (d && d.message) || 'Unknown error'
    todos.error = bridgeErr.value
    cats.error = bridgeErr.value
  })
  const boot = () => {
    if (typeof window !== 'undefined' && window.__nv?.send) {
      cats.fetchAll()
      todos.fetchAll()
      return
    }
    requestAnimationFrame(boot)
  }
  boot()
})

function onEditTodo(t) {
  editingTodo.value = t
  editModalOpen.value = true
}

function closeEditModal() {
  editModalOpen.value = false
  editingTodo.value = null
}

function onModalSubmit(payload) {
  bridgeErr.value = ''
  todos.updateOne(payload)
  closeEditModal()
}

function onDeleteTodo(id) {
  const t = todos.items.find((x) => x.id === id)
  pendingDeleteId.value = id
  pendingDeleteTitle.value = (t && t.title) || 'This task'
  deleteConfirmOpen.value = true
}

function closeDeleteConfirm() {
  deleteConfirmOpen.value = false
  pendingDeleteId.value = null
  pendingDeleteTitle.value = ''
}

function confirmDeleteTodo() {
  const id = pendingDeleteId.value
  if (id == null) return
  todos.remove(id)
  if (editingTodo.value && editingTodo.value.id === id) closeEditModal()
  closeDeleteConfirm()
}
</script>

<template>
  <div class="nv-app-shell">
    <SideToolbar v-model="activeView" />
    <div class="nv-main">
      <div class="nv-main-inner">
        <header>
          <h1 class="nv-title">Industrial Todo</h1>
          <p class="sub">Vue + SQLite via nativeView — theme from <code>theme.css</code></p>
        </header>

        <div v-if="bridgeErr || todos.error || cats.error" class="nv-error">
          {{ bridgeErr || todos.error || cats.error }}
        </div>

        <CategoriesView v-if="activeView === 'categories'" @clear-error="bridgeErr = ''" />
        <ArchiveView v-else-if="activeView === 'archive'" />
        <DoneView v-else-if="activeView === 'done'" />
        <TasksView
          v-else
          @clear-error="bridgeErr = ''"
          @edit-todo="onEditTodo"
          @delete-todo="onDeleteTodo"
          @show-done="activeView = 'done'"
          @show-archive="activeView = 'archive'"
        />

        <TodoEditModal
          :open="editModalOpen"
          :todo="editingTodo"
          :categories="cats.items"
          @submit="onModalSubmit"
          @close="closeEditModal"
        />

        <ModalDialog :open="deleteConfirmOpen" :title-id="deleteConfirmTitleId" @close="closeDeleteConfirm">
          <div class="del-pad">
            <h2 :id="deleteConfirmTitleId" class="del-h2">Delete task?</h2>
            <p class="del-msg">
              <span class="del-q">“{{ pendingDeleteTitle }}”</span>
              will be permanently removed.
            </p>
            <div class="del-actions">
              <button type="button" class="nv-btn nv-btn-ghost" @click="closeDeleteConfirm">Cancel</button>
              <button type="button" class="nv-btn nv-btn-danger" @click="confirmDeleteTodo">Delete</button>
            </div>
          </div>
        </ModalDialog>
      </div>
    </div>
  </div>
</template>

<style scoped>
.nv-app-shell {
  display: flex;
  min-height: 100vh;
  align-items: stretch;
}
.nv-main {
  flex: 1;
  min-width: 0;
}
.nv-main-inner {
  max-width: 920px;
  margin: 0 auto;
  padding: var(--nv-space-4);
}
.sub {
  color: var(--nv-muted);
  margin: 0 0 var(--nv-space-4);
}
.del-pad {
  padding: var(--nv-space-4);
}
.del-h2 {
  margin: 0 0 var(--nv-space-2);
  font-size: 1.15rem;
}
.del-msg {
  margin: 0 0 var(--nv-space-4);
  color: var(--nv-muted);
  line-height: 1.45;
}
.del-q {
  color: var(--nv-text);
  font-weight: 600;
}
.del-actions {
  display: flex;
  flex-wrap: wrap;
  gap: var(--nv-space-2);
  justify-content: flex-end;
}
</style>
