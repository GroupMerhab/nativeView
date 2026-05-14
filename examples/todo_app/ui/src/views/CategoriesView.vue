<script setup>
import { ref } from 'vue'
import { useCategoriesStore } from '../store/categories.js'

const emit = defineEmits(['clear-error'])

const cats = useCategoriesStore()
const catName = ref('')
const catColor = ref('#6366f1')

function addCategory() {
  emit('clear-error')
  cats.createOne(catName.value.trim(), catColor.value)
  catName.value = ''
}
</script>

<template>
  <section class="nv-card">
    <h2 class="h2">Categories</h2>
    <div class="cat-row">
      <input v-model="catName" class="nv-input cat-name" placeholder="Name" maxlength="64" />
      <div class="color-wrap">
        <label class="nv-label color-lbl" for="cat-color">Color</label>
        <input id="cat-color" v-model="catColor" class="nv-color" type="color" title="Category color" />
      </div>
      <button type="button" class="nv-btn nv-btn-primary" :disabled="!catName.trim()" @click="addCategory">
        Add category
      </button>
    </div>
    <ul class="cat-ul">
      <li v-for="c in cats.items" :key="c.id" class="cat-li">
        <span class="swatch" :style="{ background: c.color }" />
        <span>{{ c.name }}</span>
        <button type="button" class="nv-btn nv-btn-ghost" @click="cats.remove(c.id)">Remove</button>
      </li>
    </ul>
  </section>
</template>

<style scoped>
.h2 {
  margin: 0 0 var(--nv-space-3);
  font-size: 1.1rem;
}
.cat-row {
  display: flex;
  flex-wrap: wrap;
  gap: var(--nv-space-3);
  align-items: flex-end;
}
.cat-name {
  flex: 2 1 200px;
  max-width: 100%;
}
.color-wrap {
  display: flex;
  flex-direction: column;
  gap: var(--nv-space-1);
}
.color-lbl {
  margin: 0;
}
.nv-color {
  width: 2.75rem;
  height: 2.25rem;
  padding: 2px;
  border: 1px solid var(--nv-border);
  border-radius: var(--nv-radius-sm);
  background: var(--nv-surface);
  cursor: pointer;
}
.cat-ul {
  list-style: none;
  padding: 0;
  margin: var(--nv-space-3) 0 0;
}
.cat-li {
  display: flex;
  align-items: center;
  gap: var(--nv-space-2);
  padding: var(--nv-space-2) 0;
  border-bottom: 1px solid var(--nv-border);
}
.swatch {
  width: 1rem;
  height: 1rem;
  border-radius: 4px;
  border: 1px solid var(--nv-border);
}
</style>
