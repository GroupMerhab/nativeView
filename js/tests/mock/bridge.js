// Standalone bridge mock for tests (no deps on nativeview.js)
(function () {
  var g = (typeof window !== 'undefined') ? window
        : (typeof globalThis !== 'undefined' ? globalThis : this);
  if (!g.window) g.window = {};

  var sent = [];
  var handlers = Object.create(null);
  var autoQueue = [];

  function on(event, fn) {
    var a = handlers[event];
    if (!a) { a = []; handlers[event] = a; }
    a.push(fn);
  }

  function emit(event, rawJson) {
    var a = handlers[event];
    if (!a || a.length === 0) return;
    for (var i = 0; i < a.length; i++) {
      try { a[i](rawJson); } catch (e) { /* ignore in mock */ }
    }
  }

  function send(event, jsonStr) {
    var parsed;
    try { parsed = JSON.parse(jsonStr); } catch (e) { parsed = { __parse_error: true, raw: jsonStr }; }
    sent.push({ event: event, json: jsonStr, parsed: parsed });
    if (autoQueue.length > 0) {
      var ar = autoQueue.shift();
      var seq = (ar && ar.seq != null) ? ar.seq : (parsed && parsed.s != null ? parsed.s : 0);
      var ok = (ar && ar.ok) ? 1 : 0;
      var data = ar ? ar.data : null;
      var reply = { e: event, s: seq, ok: ok, d: data };
      emit(event, JSON.stringify(reply));
    }
  }

  function lastSent() { return sent.length ? sent[sent.length - 1] : null; }
  function allSent() { return sent.slice(); }
  function clearSent() { sent.length = 0; }
  function autoReply(seq, ok, data) { autoQueue.push({ seq: seq, ok: !!ok, data: data }); }
  function simulatePush(event, data) { emit(event, JSON.stringify({ e: event, d: data, s: 0 })); }

  g.window.__nv = {
    send: send,
    _emit: emit,
    on: on,
    _lastSent: lastSent,
    _allSent: allSent,
    _clearSent: clearSent,
    _autoReply: autoReply,
    _simulatePush: simulatePush
  };
})(); 
