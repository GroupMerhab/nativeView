import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'
import { nvSendTodo, nvOn } from './bridge.js'

describe('bridge', () => {
  beforeEach(() => {
    global.window = { __nv: { send: vi.fn(), on: vi.fn() } }
  })
  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('nvSendTodo forwards JSON string', () => {
    nvSendTodo({ action: 'todos.list' })
    expect(window.__nv.send).toHaveBeenCalledWith('todo', '{"action":"todos.list"}')
  })

  it('nvOn registers handler', () => {
    const fn = () => {}
    nvOn('todos.list', fn)
    expect(window.__nv.on).toHaveBeenCalledWith('todos.list', fn)
  })
})
