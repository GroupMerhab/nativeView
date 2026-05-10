// Promise-based RPC over IPC
// Only this file mutates _state.seq and _state.pending
var _request = {
  // Send a request to C with a timeout; returns a Promise
  send: function(event, data, timeoutMs) {
    if (!_state.ready && event !== 'app.handshake') {
      return Promise.reject(nv_err_not_ready());
    }
    var seq = _state.seq++;
    var to = (typeof timeoutMs === 'number' && timeoutMs > 0) ? timeoutMs : NV_TIMEOUT_DEFAULT_MS;
    return new Promise(function(resolve, reject) {
      var timer = setTimeout(function() {
        if (_state.pending[seq]) {
          delete _state.pending[seq];
          try { reject(nv_err_timeout(event)); } catch (e) { /* ignore */ }
        }
      }, to);
      _state.pending[seq] = { resolve: resolve, reject: reject, timer: timer };
      _ipc.send(event, data, seq);
    });
  },

  // Fire-and-forget: no seq, no timeout, no Promise
  fire: function(event, data) {
    _ipc.send(event, data, 0);
  }
};
