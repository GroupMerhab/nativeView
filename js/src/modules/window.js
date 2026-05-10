// NativeView.window.* API — window operations (depends on request.js, events.js, constants.js)
var NV_WINDOW_OPS = {
  setTitle:       "window.setTitle",
  setSize:        "window.setSize",
  getSize:        "window.getSize",
  setPosition:    "window.setPosition",
  getPosition:    "window.getPosition",
  setMinSize:     "window.setMinSize",
  center:         "window.center",
  minimize:       "window.minimize",
  maximize:       "window.maximize",
  restore:        "window.restore",
  fullscreen:     "window.fullscreen",
  isFullscreen:   "window.isFullscreen",
  close:          "window.close",
  focus:          "window.focus",
  isFocused:      "window.isFocused",
  setResizable:   "window.setResizable",
  setAlwaysOnTop: "window.setAlwaysOnTop",
  setOpacity:     "window.setOpacity",
  setZoomFactor:  "window.setZoomFactor",
  registerHotkey: "window.registerHotkey",
  unregisterHotkey: "window.unregisterHotkey"
};

var NV_WINDOW_EVENTS = {
  resized: "window.resized",
  moved:   "window.moved",
  focused: "window.focused",
  hotkeyFired: "window.hotkeyFired"
};

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};
NativeView.window = {
  /** Set the window title. */           setTitle:       function(title)   { return _request.send(NV_WINDOW_OPS.setTitle,       { title: title }); },
  /** Set the window size. */            setSize:        function(w, h)    { return _request.send(NV_WINDOW_OPS.setSize,        { w: w, h: h }); },
  /** Get the window size. */            getSize:        function()        { return _request.send(NV_WINDOW_OPS.getSize,        {}); },
  /** Set the window position. */        setPosition:    function(x, y)    { return _request.send(NV_WINDOW_OPS.setPosition,    { x: x, y: y }); },
  /** Get the window position. */        getPosition:    function()        { return _request.send(NV_WINDOW_OPS.getPosition,    {}); },
  /** Set minimum window size. */        setMinSize:     function(w, h)    { return _request.send(NV_WINDOW_OPS.setMinSize,     { w: w, h: h }); },
  /** Center the window. */              center:         function()        { return _request.send(NV_WINDOW_OPS.center,         {}); },
  /** Minimize the window. */            minimize:       function()        { return _request.send(NV_WINDOW_OPS.minimize,       {}); },
  /** Maximize the window. */            maximize:       function()        { return _request.send(NV_WINDOW_OPS.maximize,       {}); },
  /** Restore the window. */             restore:        function()        { return _request.send(NV_WINDOW_OPS.restore,        {}); },
  /** Toggle fullscreen. */              fullscreen:     function(enable)  { return _request.send(NV_WINDOW_OPS.fullscreen,     { enable: enable }); },
  /** Query fullscreen state. */         isFullscreen:   function()        { return _request.send(NV_WINDOW_OPS.isFullscreen,   {}); },
  /** Close the window. */               close:          function()        { return _request.send(NV_WINDOW_OPS.close,          {}); },
  /** Focus the window. */               focus:          function()        { return _request.send(NV_WINDOW_OPS.focus,          {}); },
  /** Query focus state. */              isFocused:      function()        { return _request.send(NV_WINDOW_OPS.isFocused,      {}); },
  /** Set resizable flag. */             setResizable:   function(enable)  { return _request.send(NV_WINDOW_OPS.setResizable,   { enable: enable }); },
  /** Set always-on-top flag. */         setAlwaysOnTop: function(enable)  { return _request.send(NV_WINDOW_OPS.setAlwaysOnTop, { enable: enable }); },
  /** Set window opacity. */             setOpacity:     function(value)   { return _request.send(NV_WINDOW_OPS.setOpacity,     { value: value }); },
  setZoomFactor: function(factor) { return _request.send(NV_WINDOW_OPS.setZoomFactor, { factor: factor }); },
  /** Register a global system hotkey for this window. */
  registerHotkey: function(id, combo) { return _request.send(NV_WINDOW_OPS.registerHotkey, { id: id, combo: combo }); },
  /** Unregister a global hotkey by id. */
  unregisterHotkey: function(id) { return _request.send(NV_WINDOW_OPS.unregisterHotkey, { id: id }); },

  /** Subscribe to resized push event. */ onResized:     function(fn)      { return _events.on(NV_WINDOW_EVENTS.resized, fn); },
  /** Subscribe to moved push event. */   onMoved:       function(fn)      { return _events.on(NV_WINDOW_EVENTS.moved,   fn); },
  /** Subscribe to focused push event. */ onFocused:     function(fn)      { return _events.on(NV_WINDOW_EVENTS.focused, fn); },
  /** Fired when a registered global hotkey is pressed. Payload: `{ id }`. */
  onHotkeyFired: function(fn) { return _events.on(NV_WINDOW_EVENTS.hotkeyFired, fn); }
};
