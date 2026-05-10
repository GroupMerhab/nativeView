// NativeView.windows.* API — multi-window management.
// Dependencies: request.js, events.js, constants.js

var _windows_self_id = null;

var NV_WINDOWS_OPS = {
  open:     "windows.open",
  close:    "windows.close",
  focus:    "windows.focus",
  list:     "windows.list",
  getInfo:  "windows.getInfo",
  setTitle: "windows.setTitle"
};

var NV_WINDOWS_EVENTS = {
  opened:      "windows.opened",
  beforeClose: "windows.beforeClose",
  closed:      "windows.closed",
  focused:     "windows.focused",
  blurred:     "windows.blurred"
};

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};

var _windows = {
  /**
   * Open a new window.
   * @param {object} cfg Window configuration.
   *   - id: {string} Unique window identifier
   *   - title: {string} Window title
   *   - url: {string} URL to load
   *   - width: {number} Window width in pixels (default: 800)
   *   - height: {number} Window height in pixels (default: 600)
   *   - minWidth: {number} Minimum width (optional)
   *   - minHeight: {number} Minimum height (optional)
   *   - resizable: {boolean} Allow window resize (default: true)
   *   - frameless: {boolean} No window frame (default: false)
   *   - transparent: {boolean} Transparent background (default: false)
   *   - devtools: {boolean} Show developer tools (default: false)
   *   - modal: {boolean} Modal window (default: false)
   * @returns {Promise} Resolves with {id, created}.
   */
  open: function(cfg) {
    cfg = cfg || {};
    // Apply defaults
    if (typeof cfg.width === 'undefined') cfg.width = 800;
    if (typeof cfg.height === 'undefined') cfg.height = 600;
    if (typeof cfg.resizable === 'undefined') cfg.resizable = true;
    if (typeof cfg.devtools === 'undefined') cfg.devtools = false;
    
    return _request.send(NV_WINDOWS_OPS.open, cfg);
  },

  /**
   * Close a window by ID.
   * @param {string} id Window ID.
   * @returns {Promise}
   */
  close: function(id) {
    return _request.send(NV_WINDOWS_OPS.close, { id: id });
  },

  /**
   * Focus a window by ID.
   * @param {string} id Window ID.
   * @returns {Promise}
   */
  focus: function(id) {
    return _request.send(NV_WINDOWS_OPS.focus, { id: id });
  },

  /**
   * List all windows.
   * @returns {Promise} Resolves with array of window objects.
   */
  list: function() {
    return _request.send(NV_WINDOWS_OPS.list, {});
  },

  /**
   * Get info for a specific window.
   * @param {string} id Window ID.
   * @returns {Promise} Resolves with window info object.
   */
  getInfo: function(id) {
    return _request.send(NV_WINDOWS_OPS.getInfo, { id: id });
  },

  /**
   * Set title for a specific window.
   * @param {string} id Window ID.
   * @param {string} title New title.
   * @returns {Promise}
   */
  setTitle: function(id, title) {
    return _request.send(NV_WINDOWS_OPS.setTitle, { id: id, title: title });
  },

  /**
   * Get this window's ID.
   * @returns {string|null} The window ID, or null if not yet initialized.
   */
  selfId: function() {
    return _windows_self_id;
  },

  /**
   * Internal method to set the self ID.
   * @param {string} id Window ID.
   * @private
   */
  _setSelfId: function(id) {
    _windows_self_id = id;
  },

  // Push Subscriptions
  onOpened:      function(fn) { return _events.on(NV_WINDOWS_EVENTS.opened,      fn); },
  onBeforeClose: function(fn) { return _events.on(NV_WINDOWS_EVENTS.beforeClose, fn); },
  onClosed:      function(fn) { return _events.on(NV_WINDOWS_EVENTS.closed,      fn); },
  onFocused:     function(fn) { return _events.on(NV_WINDOWS_EVENTS.focused,     fn); },
  onBlurred:     function(fn) { return _events.on(NV_WINDOWS_EVENTS.blurred,     fn); }
};

NativeView.windows = _windows;
