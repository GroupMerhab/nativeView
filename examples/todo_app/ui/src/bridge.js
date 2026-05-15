/**
 * nativeView __nv bridge: C receives event "todo" with d = JSON.parse(secondArg).
 */
export function nvSendTodo(d) {
  if (typeof window === 'undefined' || !window.__nv?.send) return
  window.__nv.send('todo', JSON.stringify(d))
}

/**
 * Registers for native push events. On Android the WebView injects `window.__nv` on
 * `onPageFinished`, which can run after Vue `onMounted`; if `__nv.on` is missing we defer
 * until it exists (same race as waiting for `__nv.send` before `fetchAll`).
 */
export function nvOn(event, fn) {
  if (typeof window === 'undefined') return
  const attach = () => {
    const nv = window.__nv
    if (nv && typeof nv.on === 'function') {
      nv.on(event, fn)
      return true
    }
    return false
  }
  if (attach()) return
  const loop = () => {
    if (attach()) return
    requestAnimationFrame(loop)
  }
  requestAnimationFrame(loop)
}
