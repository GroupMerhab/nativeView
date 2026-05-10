// Raw interface to window.__nv bridge
// Only this file communicates with window.__nv
// Perf: send allocates one wire object + one JSON.stringify
//       receive does one JSON.parse then hands object to router without re-parse
var _ipc = {
  // Send a message to C.
  // seq = 0 → fire-and-forget (no "s" key in the wire object)
  send: function(event, data, seq) {
    var wire = { e: event, d: data };
    if (seq && seq > 0) wire.s = seq;
    var json = JSON.stringify(wire);
    if (typeof window !== "undefined" && window.__nv && typeof window.__nv.send === "function") {
      window.__nv.send(event, json);
    } else {
      if (typeof _log !== "undefined") _log.error("IPC", "bridge missing", { event: event });
    }
  },

  // Entry point for ALL incoming messages from C.
  // Receives raw JSON string, parses once, forwards to _router.dispatch(event, obj)
  receive: function(event, rawJson) {
    var obj;
    try {
      obj = JSON.parse(rawJson);
    } catch (e) {
      if (typeof _log !== "undefined") _log.error("IPC", "invalid JSON", rawJson);
      return;
    }
    if (typeof _router !== "undefined" && _router && typeof _router.dispatch === "function") {
      _router.dispatch(event, obj);
    }
  }
};
