<script setup>
defineProps({
  name: { type: String, default: '' },
  color: { type: String, default: '#6366f1' },
})
</script>

<template>
  <span class="badge" :style="{ '--c': color }">{{ name }}</span>
</template>

<style scoped>
.badge {
  display: inline-flex;
  align-items: center;
  padding: 0.15rem 0.55rem;
  border-radius: 999px;
  font-size: 0.72rem;
  font-weight: 700;
  color: #fff;
  background: var(--c, var(--nv-accent));
  max-width: 10rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
