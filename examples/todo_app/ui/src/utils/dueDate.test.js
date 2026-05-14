import { describe, it, expect } from 'vitest'
import { dueSecondsToDatetimeLocal, datetimeLocalToDueSeconds, formatDueDateLabel } from './dueDate.js'

describe('dueDate', () => {
  it('dueSecondsToDatetimeLocal returns empty for zero', () => {
    expect(dueSecondsToDatetimeLocal(0)).toBe('')
    expect(dueSecondsToDatetimeLocal(null)).toBe('')
  })

  it('round-trips local wall time for a fixed unix second', () => {
    const sec = 1700000000
    const local = dueSecondsToDatetimeLocal(sec)
    expect(local).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}(:\d{2})?$/)
    expect(datetimeLocalToDueSeconds(local)).toBe(sec)
  })

  it('datetimeLocalToDueSeconds returns 0 for empty', () => {
    expect(datetimeLocalToDueSeconds('')).toBe(0)
    expect(datetimeLocalToDueSeconds(null)).toBe(0)
  })

  it('formatDueDateLabel returns empty when unset', () => {
    expect(formatDueDateLabel(0)).toBe('')
  })

  it('formatDueDateLabel is non-empty for valid second', () => {
    expect(formatDueDateLabel(1700000000).length).toBeGreaterThan(4)
  })
})
