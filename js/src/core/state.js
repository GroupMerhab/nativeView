// Single mutable state object for the entire library
// Depends on constants.js being loaded before this file
var _state = {
  // Next sequence id (integer, increment only)
  seq: 1,
  // Pending requests: seq(int) → { resolve, reject, timer }
  pending: Object.create(null),
  // Push event handlers: event(string) → Array of functions
  handlers: Object.create(null),
  // True after successful handshake with C side
  ready: false,
  // C library version string (set on handshake)
  cVersion: null,
  // Platform identifier: "mac" | "win" | "linux" (set on handshake)
  platform: null
};

// Reset to initial values — used by tests and re-connect
function _state_reset() {
  _state.seq = 1;
  _state.pending = Object.create(null);
  _state.handlers = Object.create(null);
  _state.ready = false;
  _state.cVersion = null;
  _state.platform = null;
}
