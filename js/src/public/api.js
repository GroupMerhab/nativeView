// Final assembly of the NativeView global (built last; all deps already in scope)
var NativeView = typeof NativeView !== "undefined" ? NativeView : {};

// Meta
NativeView.version = NV_VERSION;
NativeView.wireVersion = NV_WIRE_VERSION;

// Core
NativeView.send   = function(event, data)     { _request.fire(event, data); };   // from core/request.js
NativeView.invoke = function(event, data, ms) { return _request.send(event, data, ms); }; // from core/request.js
NativeView.on     = function(event, fn)       { _events.on(event, fn); };        // from core/events.js
NativeView.off    = function(event, fn)       { _events.off(event, fn); };       // from core/events.js
NativeView.once   = function(event, fn)       { _events.once(event, fn); };      // from core/events.js

// Modules
NativeView.window       = NativeView.window;        // js/src/modules/window.js
NativeView.app          = NativeView.app;           // js/src/modules/app.js
NativeView.fs           = NativeView.fs;            // js/src/modules/fs.js
NativeView.dialog       = NativeView.dialog;        // js/src/modules/dialog.js
NativeView.clipboard    = NativeView.clipboard;     // js/src/modules/clipboard.js
NativeView.shell        = NativeView.shell;         // js/src/modules/shell.js
NativeView.screen       = NativeView.screen;        // js/src/modules/screen.js
NativeView.notification = NativeView.notification;  // js/src/modules/notification.js
NativeView.tray         = NativeView.tray;          // js/src/modules/tray.js
NativeView.windows = _windows;    // from modules/windows.js
NativeView.ipc     = _ipc_bus;    // from modules/ipc_bus.js

// Errors
NativeView.NVError = NVError; // from core/error.js

// Expose internals helpful for host pages and demos
NativeView._debug = (typeof _debug !== 'undefined') ? _debug : undefined; // from debug/debug.js
NativeView._init = _init;
NativeView._state = _state;

// Expose
if (typeof module !== "undefined") {
  // Also attach to global for convenience in test/dev environments
  if (typeof global !== "undefined") global.NativeView = NativeView;
  if (typeof globalThis !== "undefined") globalThis.NativeView = NativeView;
  // Also expose _init on global for environments that expect it
  if (typeof global !== "undefined") global._init = _init;
  if (typeof globalThis !== "undefined") globalThis._init = _init;
  module.exports = NativeView;
} else {
  var g = (typeof globalThis !== "undefined") ? globalThis : (typeof window !== "undefined" ? window : this);
  g.NativeView = NativeView;
  // Also expose _init on window for host pages that call it directly
  g._init = _init;
}

// Auto-connect: attempt handshake with C-side bridge when window.__nv is available
(function() {
  if (typeof window !== "undefined") {
    // Use a small delay to ensure C-side injection is complete
    var attemptConnect = function() {
      if (window.__nv) {
        try {
          if (typeof _init !== 'undefined' && _init && typeof _init.connect === 'function') {
            _init.connect();
          } else if (typeof NativeView !== 'undefined' && NativeView && typeof NativeView.invoke === 'function') {
            // Fallback: directly send handshake if _init is not exposed
            NativeView.invoke('app.handshake', { jsVersion: NV_VERSION, wireVersion: NV_WIRE_VERSION }, NV_TIMEOUT_HANDSHAKE_MS).catch(function(){});
          }
        } catch(e) {}
      } else {
        // Retry a few more times if bridge not yet ready
        setTimeout(attemptConnect, 100);
      }
    };
    setTimeout(attemptConnect, 0);
  }
})();
