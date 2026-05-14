import { describe, it, expect } from 'vitest'
import { mount } from '@vue/test-utils'
import CategoryBadge from './components/CategoryBadge.vue'

describe('CategoryBadge', () => {
  it('renders name', () => {
    const w = mount(CategoryBadge, { props: { name: 'Work', color: '#ff0000' } })
    expect(w.text()).toContain('Work')
  })
})
