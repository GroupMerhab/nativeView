/**
 * nativeView __nv bridge: C receives event "todo" with d = JSON.parse(secondArg).
 */
export function nvSendTodo(d) {
  if (typeof window === 'undefined' || !window.__nv?.send) return
  window.__nv.send('todo', JSON.stringify(d))
}

export function nvOn(event, fn) {
  if (typeof window === 'undefined' || !window.__nv?.on) return
  window.__nv.on(event, fn)
}
