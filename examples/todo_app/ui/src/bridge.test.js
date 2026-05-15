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

  it('nvOn defers until __nv.on exists (WebView inject after Vue mount)', async () => {
    global.window = { __nv: { send: vi.fn() } }
    const handler = vi.fn()
    nvOn('todos.list', handler)
    expect(window.__nv.on).toBeUndefined()
    window.__nv.on = vi.fn()
    await new Promise((resolve) => {
      requestAnimationFrame(() => {
        requestAnimationFrame(resolve)
      })
    })
    expect(window.__nv.on).toHaveBeenCalledWith('todos.list', handler)
  })
})
