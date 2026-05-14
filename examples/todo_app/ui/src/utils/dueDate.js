/**
 * Backend stores due dates as Unix epoch seconds (non-zero = set).
 * datetime-local uses local wall time without timezone suffix.
 */
export function dueSecondsToDatetimeLocal(sec) {
  if (sec == null || sec === 0) return ''
  const n = Number(sec)
  if (!Number.isFinite(n) || n <= 0) return ''
  const d = new Date(n * 1000)
  const pad = (x) => String(x).padStart(2, '0')
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}

export function datetimeLocalToDueSeconds(str) {
  if (str == null || str === '') return 0
  const ms = Date.parse(str)
  if (!Number.isFinite(ms)) return 0
  return Math.floor(ms / 1000)
}

export function formatDueDateLabel(sec, locale = undefined) {
  if (sec == null || sec === 0) return ''
  const n = Number(sec)
  if (!Number.isFinite(n) || n <= 0) return ''
  return new Date(n * 1000).toLocaleString(locale, {
    dateStyle: 'medium',
    timeStyle: 'short',
  })
}
