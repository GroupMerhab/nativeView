// Push event subscription management
// Only this file writes to _state.handlers
var _events = {
  // Add handler for event
  on: function(event, fn) {
    var a = _state.handlers[event];
    if (!a) { a = []; _state.handlers[event] = a; }
    a.push(fn);
  },

  // Remove specific handler for event (supports removing once-wrappers)
  off: function(event, fn) {
    var a = _state.handlers[event];
    if (!a) return;
    for (var i = a.length - 1; i >= 0; i--) {
      var h = a[i];
      if (h === fn || h._nv_once === fn) a.splice(i, 1);
    }
    if (a.length === 0) delete _state.handlers[event];
  },

  // Add handler that auto-removes after first call
  once: function(event, fn) {
    function wrap(data) {
      _events.off(event, wrap);
      try { fn(data); } catch (e) { if (typeof _log !== "undefined") _log.error("events", "once handler error", e); }
    }
    wrap._nv_once = fn;
    var a = _state.handlers[event];
    if (!a) { a = []; _state.handlers[event] = a; }
    a.push(wrap);
  },

  // Remove all handlers for event
  offAll: function(event) {
    if (event in _state.handlers) delete _state.handlers[event];
  }
};
