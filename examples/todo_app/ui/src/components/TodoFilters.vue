<script setup>
import { computed } from 'vue'

const props = defineProps({
  status: { type: String, default: '' },
  priority: { type: String, default: '' },
})

const emit = defineEmits(['update:status', 'update:priority'])

const st = computed({
  get: () => props.status,
  set: (v) => emit('update:status', v),
})
const pr = computed({
  get: () => props.priority,
  set: (v) => emit('update:priority', v),
})
</script>

<template>
  <div class="nv-toolbar">
    <div class="nv-field" style="flex: 0 1 160px">
      <span class="nv-label">Filter status</span>
      <select v-model="st" class="nv-select">
        <option value="">All</option>
        <option value="pending">Pending</option>
        <option value="in_progress">In progress</option>
        <option value="cancelled">Cancelled</option>
      </select>
    </div>
    <div class="nv-field" style="flex: 0 1 160px">
      <span class="nv-label">Filter priority</span>
      <select v-model="pr" class="nv-select">
        <option value="">All</option>
        <option value="low">Low</option>
        <option value="medium">Medium</option>
        <option value="high">High</option>
        <option value="urgent">Urgent</option>
      </select>
    </div>
  </div>
</template>
