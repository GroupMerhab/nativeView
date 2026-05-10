/* NV_DEBUG_START */
// Runtime debug tools (depends on state.js, log.js). Stripped from minified build.
var _debug = (function(){
  function safeStringify(obj) {
    try { return JSON.stringify(obj, null, 2); }
    catch (e) {
      try {
        var shallow = {};
        for (var k in obj) { if (Object.prototype.hasOwnProperty.call(obj, k)) shallow[k] = obj[k]; }
        return JSON.stringify(shallow, null, 2);
      } catch (e2) { return '[Unstringifiable]'; }
    }
  }

  function now() { return Date.now ? Date.now() : new Date().getTime(); }

  return {
    // Dump entire _state to console (JSON.stringify with indent)
    dumpState: function() {
      try { _log.debug('STATE\n' + safeStringify(_state)); } catch (e) { /* ignore */ }
    },

    // Dump all in-flight requests with their ages in ms
    dumpPending: function() {
      try {
        var lines = [];
        var pend = _state && _state.pending ? _state.pending : {};
        for (var s in pend) {
          if (!Object.prototype.hasOwnProperty.call(pend, s)) continue;
          var entry = pend[s] || {};
          var t0 = entry.t || entry.ts || 0;
          var age = t0 ? (now() - t0) : -1;
          lines.push('seq=' + s + ' ageMs=' + age);
        }
        _log.debug('PENDING\n' + lines.join('\n'));
      } catch (e) { /* ignore */ }
    },

    // Dump all registered push event handler counts per event
    dumpHandlers: function() {
      try {
        var h = _state && _state.handlers ? _state.handlers : {};
        var lines = [];
        for (var ev in h) {
          if (!Object.prototype.hasOwnProperty.call(h, ev)) continue;
          var arr = h[ev] || [];
          lines.push(ev + ': ' + (arr.length|0));
        }
        _log.debug('HANDLERS\n' + lines.join('\n'));
      } catch (e) { /* ignore */ }
    },

    // Inject a fake C push event (for testing without C running)
    simulatePush: function(event, dataObj) {
      try {
        var payload = { e: event, s: 0, d: dataObj || null };
        if (typeof _router !== 'undefined' && _router && typeof _router._pushEvent === 'function') {
          _router._pushEvent(payload);
          return;
        }
        // Fallback: invoke registered handlers directly
        var hs = _state && _state.handlers ? _state.handlers[event] : null;
        if (hs && hs.length) {
          for (var i=0; i<hs.length; i++) {
            try { hs[i](payload.d); } catch (e) { /* ignore */ }
          }
        }
      } catch (e) { /* ignore */ }
    },

    // Inject a fake C reply to a pending request
    simulateReply: function(seq, ok, dataObj) {
      try {
        // Prefer routing through IPC if available to mimic real flow
        if (typeof _ipc !== 'undefined' && _ipc && typeof _ipc.receive === 'function') {
          var reply = { e: '', s: seq, ok: ok ? 1 : 0, d: dataObj || null };
          _ipc.receive('', JSON.stringify(reply));
          return;
        }
        // Fallback: resolve/reject directly via pending map
        var pend = _state && _state.pending ? _state.pending : {};
        var p = pend[seq];
        if (!p) return;
        if (ok) { if (typeof p.resolve === 'function') p.resolve(dataObj || null); }
        else { if (typeof p.reject === 'function') p.reject(dataObj || null); }
        try { delete pend[seq]; } catch (e) { pend[seq] = undefined; }
      } catch (e) { /* ignore */ }
    }
  };
})();
/* NV_DEBUG_END */
