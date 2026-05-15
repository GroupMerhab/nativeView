// Initialize and connect to the native bridge; performs version handshake
// Idempotent: safe to call multiple times
var _init = {
  connect: function() {
    if (_state.ready) return;

    var w = (typeof window !== "undefined") ? window : null;
    if (!w || !w.__nv) {
      if (typeof _log !== "undefined") _log.debug("init", "no window.__nv", { hasWindow: !!w });
      if (typeof _log !== "undefined") _log.error("init", "bridge missing");
      return;
    }

    // __nv._emit is wired in public/api.js (nv_send / C push uses {e,d} via _ipc.receive).

    // Send handshake
    if (typeof _log !== "undefined") _log.debug("init", "sending handshake");
    var payload = { jsVersion: NV_VERSION, wireVersion: NV_WIRE_VERSION };
    _request.send('app.handshake', payload, NV_TIMEOUT_HANDSHAKE_MS)
      .then(function(reply) {
        if (!reply || reply.wireVersion !== NV_WIRE_VERSION) {
          var err = nv_err_version(reply && reply.wireVersion, NV_WIRE_VERSION);
          if (typeof _log !== "undefined") _log.error("init", "handshake version mismatch", err);
          return;
        }
        if (typeof _log !== "undefined") _log.debug("init", "handshake ok", reply);
        _state.cVersion = reply.cVersion || null;
        _state.platform = reply.platform || null;
        
        // MW: set window ids on handshake
        if (reply.windowId) {
          if (typeof _windows !== "undefined") _windows._setSelfId(reply.windowId);
          if (typeof _ipc_bus !== "undefined") _ipc_bus._setSelfId(reply.windowId);
        }

        _state.ready = true;
      })
      .catch(function(err) {
        if (typeof _log !== "undefined") _log.error("init", "handshake failed", err);
      });
  }
};
