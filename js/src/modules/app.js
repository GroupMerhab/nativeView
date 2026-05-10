// NativeView.app API — application operations (depends on request.js, constants.js)
var NV_APP_OPS = {
  quit:           "app.quit",
  getVersion:     "app.getVersion",
  getName:        "app.getName",
  getDataDir:     "app.getDataDir",
  getExePath:     "app.getExePath",
  getResourceDir: "app.getResourceDir",
  getPlatform:    "app.getPlatform",
  getLocale:      "app.getLocale",
  setMenu:        "app.setMenu"
};

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};
NativeView.app = {
  /** Quit the application (fire-and-forget). */   quit:           function()      { return _request.fire(NV_APP_OPS.quit,           {}); },
  /** Get app version. */                          getVersion:     function()      { return _request.send(NV_APP_OPS.getVersion,     {}); },
  /** Get app name. */                             getName:        function()      { return _request.send(NV_APP_OPS.getName,        {}); },
  /** Get user data directory. */                  getDataDir:     function()      { return _request.send(NV_APP_OPS.getDataDir,     {}); },
  /** Get executable path. */                      getExePath:     function()      { return _request.send(NV_APP_OPS.getExePath,     {}); },
  /** Get resources directory. */                  getResourceDir: function()      { return _request.send(NV_APP_OPS.getResourceDir, {}); },
  /** Get platform identifier. */                  getPlatform:    function()      { return _request.send(NV_APP_OPS.getPlatform,    {}); },
  /** Get current locale. */                       getLocale:      function()      { return _request.send(NV_APP_OPS.getLocale,      {}); },
  /** Set native app/window menu from item tree (array or `{ items: [...] }`). */
  setMenu:        function(items)               { return _request.send(NV_APP_OPS.setMenu,        items); }
};
