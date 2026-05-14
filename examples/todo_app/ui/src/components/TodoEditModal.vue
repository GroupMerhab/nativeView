<script setup>
import ModalDialog from './ModalDialog.vue'
import TodoForm from './TodoForm.vue'

defineProps({
  open: { type: Boolean, default: false },
  todo: { type: Object, default: null },
  categories: { type: Array, default: () => [] },
})

const emit = defineEmits(['submit', 'close'])

const titleId = 'todo-edit-modal-title'

function onSubmit(payload, isEdit) {
  emit('submit', payload, isEdit)
}

function onClose() {
  emit('close')
}
</script>

<template>
  <ModalDialog :open="open && !!todo" :title-id="titleId" @close="onClose">
    <div class="pad">
      <TodoForm
        v-if="todo"
        :categories="categories"
        :editing="todo"
        variant="flat"
        id-prefix="edit-todo-"
        :title-id="titleId"
        @submit="onSubmit"
        @cancel-edit="onClose"
      />
    </div>
  </ModalDialog>
</template>

<style scoped>
.pad {
  padding: var(--nv-space-4);
}
</style>
