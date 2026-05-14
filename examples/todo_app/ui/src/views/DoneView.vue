<script setup>
import { computed, shallowRef, watch } from 'vue'
import CategoryBadge from '../components/CategoryBadge.vue'
import { useCategoriesStore } from '../store/categories.js'
import { useTodosStore } from '../store/todos.js'
import { formatDueDateLabel } from '../utils/dueDate.js'

const cats = useCategoriesStore()
const todos = useTodosStore()

const selected = shallowRef(new Set())

watch(
  () => todos.doneTodos.map((t) => t.id).join(','),
  () => {
    const allowed = new Set(todos.doneTodos.map((t) => t.id))
    const next = new Set()
    for (const id of selected.value) {
      if (allowed.has(id)) next.add(id)
    }
    selected.value = next
  },
)

const rows = computed(() => todos.doneTodos)

const allSelected = computed(() => {
  const r = rows.value
  if (!r.length) return false
  return r.every((t) => selected.value.has(t.id))
})

const someSelected = computed(() => selected.value.size > 0)

function categoryFor(todo) {
  const id = todo.categoryId
  if (id == null) return null
  return cats.items.find((c) => c.id === id) || null
}

function dueLabel(todo) {
  return formatDueDateLabel(todo.dueDate)
}

function toggleRow(id, checked) {
  const next = new Set(selected.value)
  if (checked) next.add(id)
  else next.delete(id)
  selected.value = next
}

function toggleSelectAll(checked) {
  if (!checked) {
    selected.value = new Set()
    return
  }
  selected.value = new Set(rows.value.map((t) => t.id))
}

function onArchive() {
  if (!selected.value.size) return
  todos.archiveDoneIds([...selected.value])
  selected.value = new Set()
}
</script>

<template>
  <section class="nv-card">
    <div class="head">
      <h2 class="h2">Done</h2>
      <button
        type="button"
        class="nv-btn nv-btn-primary"
        :disabled="!someSelected"
        @click="onArchive"
      >
        Archive selected
      </button>
    </div>
    <p class="hint">Completed tasks. Archive removes them from the main task list.</p>

    <p v-if="!rows.length" class="empty">No completed todos yet.</p>

    <div v-else class="wrap">
      <table class="tbl" aria-label="Done todos">
        <thead>
          <tr>
            <th class="col-check" scope="col">
              <input
                type="checkbox"
                :checked="allSelected"
                aria-label="Select all done todos"
                @change="toggleSelectAll($event.target.checked)"
              />
            </th>
            <th scope="col">Title</th>
            <th scope="col">Category</th>
            <th scope="col">Priority</th>
            <th scope="col">Due</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="t in rows" :key="t.id">
            <td class="col-check">
              <input
                type="checkbox"
                :checked="selected.has(t.id)"
                :aria-label="`Select ${t.title}`"
                @change="toggleRow(t.id, $event.target.checked)"
              />
            </td>
            <td class="td-title">{{ t.title }}</td>
            <td>
              <CategoryBadge v-if="categoryFor(t)" :name="categoryFor(t).name" :color="categoryFor(t).color" />
              <span v-else class="muted">—</span>
            </td>
            <td><span class="pill">{{ t.priority }}</span></td>
            <td>
              <span v-if="dueLabel(t)" class="pill due">{{ dueLabel(t) }}</span>
              <span v-else class="muted">—</span>
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </section>
</template>

<style scoped>
.h2 {
  margin: 0;
  font-size: 1.1rem;
}
.head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--nv-space-3);
  flex-wrap: wrap;
  margin-bottom: var(--nv-space-2);
}
.hint {
  margin: 0 0 var(--nv-space-3);
  color: var(--nv-muted);
  font-size: 0.9rem;
}
.empty {
  color: var(--nv-muted);
  margin: var(--nv-space-3) 0;
}
.wrap {
  overflow-x: auto;
  margin-top: var(--nv-space-2);
}
.tbl {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.92rem;
}
.tbl th,
.tbl td {
  padding: var(--nv-space-2) var(--nv-space-3);
  text-align: left;
  border-bottom: 1px solid var(--nv-border);
  vertical-align: middle;
}
.tbl th {
  font-weight: 600;
  color: var(--nv-muted);
  font-size: 0.75rem;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  background: var(--nv-surface-2);
}
.tbl tbody tr:hover {
  background: rgba(99, 102, 241, 0.04);
}
.col-check {
  width: 2.5rem;
  text-align: center;
}
.col-check input {
  cursor: pointer;
}
.td-title {
  font-weight: 600;
  max-width: 280px;
}
.muted {
  color: var(--nv-muted);
}
.pill {
  font-size: 0.72rem;
  font-weight: 700;
  padding: 0.12rem 0.45rem;
  border-radius: 6px;
  background: #fef3c7;
  color: var(--nv-warning);
  text-transform: capitalize;
}
.pill.due {
  background: #ecfdf5;
  color: var(--nv-success);
  text-transform: none;
  font-weight: 600;
}
</style>
