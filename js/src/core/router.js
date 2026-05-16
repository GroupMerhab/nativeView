// Router for incoming messages (hot path)
// Perf contract:
// - dispatch: zero allocations, branches only; forwards based on msg.s
// - _resolvePromise: minimal work, no new objects except NVError on rejection
// - _pushEvent: single allocation for handler snapshot via slice(); no others
var _router = {
  // Accepts either (msg) or (event, msg) for compatibility with _ipc.receive tests
  dispatch: function(a, b) {
    var msg = (b !== undefined) ? b : a;
    if (!msg) return;
    var s = msg.s || 0;
    if (s > 0) {
      _router._resolvePromise(msg);
    } else {
      _router._pushEvent(msg);
    }
  },

  // Resolve/reject pending promise by seq id; clears timer and deletes entry
  _resolvePromise: function(msg) {
    var seq = msg.s;
    var entry = _state.pending[seq];
    if (!entry) return;
    if (entry.timer) { try { clearTimeout(entry.timer); } catch (e) {} }
    delete _state.pending[seq];
    if (msg.ok === 1 || msg.ok === true) {
      try { entry.resolve(msg.d); } catch (e1) { if (typeof _log !== "undefined") _log.error("router", "resolve thrown", e1); }
    } else {
      var err = nv_error_from_payload(msg.d);
      try { entry.reject(err); } catch (e2) { if (typeof _log !== "undefined") _log.error("router", "reject thrown", e2); }
    }
  },

  // Deliver push event to all registered handlers using a snapshot
  _pushEvent: function(msg) {
    var ev = msg.e;
    var arr = _state.handlers[ev];
    if (!arr || arr.length === 0) return;
    var snap = arr.slice(); // single allowed allocation on hot path
    for (var i = 0; i < snap.length; i++) {
      var fn = snap[i];
      try { fn(msg.d); }
      catch (e) { if (typeof _log !== "undefined") _log.error("router", "handler error", e); }
    }
  }
};
