<script setup>
import { computed, reactive, ref, watch } from 'vue'
import { dueSecondsToDatetimeLocal, datetimeLocalToDueSeconds } from '../utils/dueDate.js'

const props = defineProps({
  categories: { type: Array, default: () => [] },
  editing: { type: Object, default: null },
  variant: { type: String, default: 'card' },
  idPrefix: { type: String, default: 'todo-' },
  titleId: { type: String, default: '' },
})

const emit = defineEmits(['submit', 'cancel-edit'])

const form = reactive({
  title: '',
  description: '',
  status: 'pending',
  priority: 'medium',
  categoryId: 0,
  dueDate: 0,
})

const dueLocal = ref('')

watch(
  () => props.editing,
  (e) => {
    if (e) {
      form.title = e.title || ''
      form.description = e.description || ''
      form.status = e.status || 'pending'
      form.priority = e.priority || 'medium'
      form.categoryId = e.categoryId ?? 0
      form.dueDate = e.dueDate ?? 0
      dueLocal.value = dueSecondsToDatetimeLocal(form.dueDate)
    } else {
      form.title = ''
      form.description = ''
      form.status = 'pending'
      form.priority = 'medium'
      form.categoryId = 0
      form.dueDate = 0
      dueLocal.value = ''
    }
  },
  { immediate: true },
)

const heading = computed(() => (props.editing ? 'Edit todo' : 'New todo'))

const sectionClass = computed(() =>
  props.variant === 'flat' ? 'form-flat' : 'form-card nv-card',
)

function onSubmit() {
  const dueSec = datetimeLocalToDueSeconds(dueLocal.value)
  const payload = {
    title: form.title.trim(),
    description: form.description || '',
    status: form.status,
    priority: form.priority,
    categoryId: form.categoryId || 0,
    parentId: 0,
    dueDate: dueSec,
  }
  if (props.editing) {
    payload.id = props.editing.id
    payload.position = props.editing.position ?? 0
  }
  emit('submit', payload, !!props.editing)
  if (!props.editing) {
    form.title = ''
    form.description = ''
    dueLocal.value = ''
  }
}
</script>

<template>
  <section :class="sectionClass">
    <h2 class="h2" :id="titleId || undefined">{{ heading }}</h2>
    <form @submit.prevent="onSubmit">
      <div class="nv-row">
        <div class="nv-field" style="flex: 2 1 220px">
          <label class="nv-label" :for="`${idPrefix}title`">Title</label>
          <input :id="`${idPrefix}title`" v-model="form.title" class="nv-input" required maxlength="255" />
        </div>
        <div class="nv-field">
          <label class="nv-label" :for="`${idPrefix}cat`">Category</label>
          <select :id="`${idPrefix}cat`" v-model.number="form.categoryId" class="nv-select">
            <option :value="0">— None —</option>
            <option v-for="c in categories" :key="c.id" :value="c.id">{{ c.name }}</option>
          </select>
        </div>
      </div>
      <div class="nv-field" style="margin-top: var(--nv-space-3)">
        <label class="nv-label" :for="`${idPrefix}desc`">Description</label>
        <textarea :id="`${idPrefix}desc`" v-model="form.description" class="nv-textarea" maxlength="4096" />
      </div>
      <div class="nv-row" style="margin-top: var(--nv-space-3)">
        <div class="nv-field">
          <label class="nv-label" :for="`${idPrefix}st`">Status</label>
          <select :id="`${idPrefix}st`" v-model="form.status" class="nv-select">
            <option value="pending">Pending</option>
            <option value="in_progress">In progress</option>
            <option value="done">Done</option>
            <option value="cancelled">Cancelled</option>
          </select>
        </div>
        <div class="nv-field">
          <label class="nv-label" :for="`${idPrefix}pr`">Priority</label>
          <select :id="`${idPrefix}pr`" v-model="form.priority" class="nv-select">
            <option value="low">Low</option>
            <option value="medium">Medium</option>
            <option value="high">High</option>
            <option value="urgent">Urgent</option>
          </select>
        </div>
        <div class="nv-field">
          <label class="nv-label" :for="`${idPrefix}due`">Due date</label>
          <input :id="`${idPrefix}due`" v-model="dueLocal" type="datetime-local" class="nv-input" />
        </div>
      </div>
      <div class="actions">
        <button type="submit" class="nv-btn nv-btn-primary">{{ editing ? 'Save' : 'Add todo' }}</button>
        <button v-if="editing" type="button" class="nv-btn nv-btn-ghost" @click="emit('cancel-edit')">
          Cancel
        </button>
      </div>
    </form>
  </section>
</template>

<style scoped>
.h2 {
  margin: 0 0 var(--nv-space-3);
  font-size: 1.1rem;
}
.actions {
  margin-top: var(--nv-space-3);
  display: flex;
  gap: var(--nv-space-2);
}
.form-flat {
  margin: 0;
}
.form-flat .h2 {
  margin-top: 0;
}
</style>
