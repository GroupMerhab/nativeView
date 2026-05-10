// NativeView.ipc.* API — inter-window messaging.
// Dependencies: request.js, events.js, constants.js, modules/windows.js

var _ipc_bus_self_id = null;
var _ipc_handlers = Object.create(null);

var NV_IPC_BUS_OPS = {
  send: "ipc_bus.send"
};

var NV_IPC_BUS_MSG_EVENT = "ipc_bus.message";

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};

var _ipc_bus = {
  /**
   * Send a message to a specific window.
   * @param {string} toWindowId Target window ID.
   * @param {string} event Event name.
   * @param {*} data Message payload.
   * @returns {Promise} Resolves with {delivered: 0|1}.
   */
  send: function(toWindowId, event, data) {
    return _request.send(NV_IPC_BUS_OPS.send, { to: toWindowId, event: event, data: data });
  },

  /**
   * Broadcast a message to all windows.
   * @param {string} event Event name.
   * @param {*} data Message payload.
   * @returns {Promise} Resolves with {delivered: N}.
   */
  broadcast: function(event, data) {
    return this.send("*", event, data);
  },

  /**
   * Register a handler for an IPC event.
   * @param {string} event Event name.
   * @param {function} fn Handler function (data, fromWindowId).
   */
  on: function(event, fn) {
    var a = _ipc_handlers[event];
    if (!a) { a = []; _ipc_handlers[event] = a; }
    a.push(fn);
  },

  /**
   * Remove a handler for an IPC event.
   * @param {string} event Event name.
   * @param {function} fn Handler function.
   */
  off: function(event, fn) {
    var a = _ipc_handlers[event];
    if (!a) return;
    for (var i = a.length - 1; i >= 0; i--) {
      var h = a[i];
      if (h === fn || h._nv_once === fn) a.splice(i, 1);
    }
    if (a.length === 0) delete _ipc_handlers[event];
  },

  /**
   * Register a one-time handler for an IPC event.
   * @param {string} event Event name.
   * @param {function} fn Handler function (data, fromWindowId).
   */
  once: function(event, fn) {
    var self = this;
    function wrap(data, from) {
      self.off(event, wrap);
      try { fn(data, from); } catch (e) { if (typeof _log !== "undefined") _log.error("ipc_bus", "once handler error", e); }
    }
    wrap._nv_once = fn;
    var a = _ipc_handlers[event];
    if (!a) { a = []; _ipc_handlers[event] = a; }
    a.push(wrap);
  },

  /**
   * Get this window's ID.
   * @returns {string|null} The window ID.
   */
  selfId: function() {
    return _ipc_bus_self_id;
  },

  /**
   * Internal method to set the self ID.
   * @param {string} id Window ID.
   * @private
   */
  _setSelfId: function(id) {
    _ipc_bus_self_id = id;
  }
};

NativeView.ipc = _ipc_bus;

// Internal dispatch for incoming IPC messages
_events.on(NV_IPC_BUS_MSG_EVENT, function(payload) {
  // Payload structure: { from, event, data }
  if (!payload || !payload.event) return;
  
  var handlers = _ipc_handlers[payload.event];
  if (!handlers) return;
  
  // Snapshot before iteration to protect against removal during dispatch
  var snap = handlers.slice();
  for (var i = 0; i < snap.length; i++) {
    try {
      snap[i](payload.data, payload.from);
    } catch (e) {
      if (typeof _log !== "undefined") _log.error("ipc_bus", "handler threw", e);
    }
  }
});
