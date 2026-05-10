// NativeView.shell API — shell operations (depends on request.js, constants.js)
var NV_SHELL_OPS = {
  openUrl:      "shell.openUrl",
  openPath:     "shell.openPath",
  showInFolder: "shell.showInFolder",
  exec:         "shell.exec"
};

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};
NativeView.shell = {
  /** Open URL with default handler. */      openUrl:      function(url)            { return _request.fire(NV_SHELL_OPS.openUrl,      { url: url }); },
  /** Open file or folder path. */          openPath:     function(path)           { return _request.fire(NV_SHELL_OPS.openPath,     { path: path }); },
  /** Reveal file in folder. */             showInFolder: function(path)           { return _request.fire(NV_SHELL_OPS.showInFolder, { path: path }); },
  /** Execute command (returns result). */  exec:         function(cmd, args, cwd) { return _request.send(NV_SHELL_OPS.exec,         { cmd: cmd, args: args, cwd: cwd }); }
};
