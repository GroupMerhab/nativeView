var NV_SCREEN_OPS = {
  getPrimary:  "screen.getPrimary",
  getAll:      "screen.getAll",
  getCursorPos:"screen.getCursorPos"
};

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};
NativeView.screen = {
  getPrimary:  function() { return _request.send(NV_SCREEN_OPS.getPrimary,   {}); },
  getAll:      function() { return _request.send(NV_SCREEN_OPS.getAll,       {}); },
  getCursorPos:function() { return _request.send(NV_SCREEN_OPS.getCursorPos, {}); }
};
