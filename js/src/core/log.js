// Internal logging utilities for nativeview.js
// Depends on constants.js; debug blocks are stripped by js_minify.py
var _log = {
  // Debug logger: only present in development builds (removed by minifier)
  debug: function(context, msg, data) {
    /* NV_DEBUG_START */
    if (typeof console !== "undefined") {
      console.debug("[NV:" + context + "]", msg, data !== undefined ? data : "");
    }
    /* NV_DEBUG_END */
  },

  // Error logger: always present in production
  error: function(context, msg, data) {
    if (typeof console !== "undefined") {
      console.error("[NV ERROR:" + context + "]", msg, data !== undefined ? data : "");
    }
  }
};
