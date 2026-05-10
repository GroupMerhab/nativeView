// NativeView.fs API — filesystem operations (depends on request.js, events.js, constants.js)
var NV_FS_OPS = {
  readText:    "fs.readText",
  writeText:   "fs.writeText",
  readBinary:  "fs.readBinary",
  writeBinary: "fs.writeBinary",
  exists:      "fs.exists",
  remove:      "fs.remove",
  move:        "fs.move",
  copy:        "fs.copy",
  stat:        "fs.stat",
  readDir:     "fs.readDir",
  mkdir:       "fs.mkdir",
  rmdir:       "fs.rmdir",
  watch:       "fs.watch",
  unwatch:     "fs.unwatch"
};

var NV_FS_EVENTS = {
  changed: "fs.changed"
};

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};
NativeView.fs = {
  /** Read text file. */                 readText:    function(path)            { return _request.send(NV_FS_OPS.readText,    { path: path }); },
  /** Write text file. */                writeText:   function(path, text)      { return _request.send(NV_FS_OPS.writeText,   { path: path, text: text }); },
  /** Read binary as base64. */          readBinary:  function(path)            { return _request.send(NV_FS_OPS.readBinary,  { path: path }); },
  /** Write binary from base64. */       writeBinary: function(path, b64)       { return _request.send(NV_FS_OPS.writeBinary, { path: path, b64: b64 }); },
  /** Check existence. */                exists:      function(path)            { return _request.send(NV_FS_OPS.exists,      { path: path }); },
  /** Remove file/dir. */                remove:      function(path)            { return _request.send(NV_FS_OPS.remove,      { path: path }); },
  /** Move file/dir. */                  move:        function(src, dst)        { return _request.send(NV_FS_OPS.move,        { src: src, dst: dst }); },
  /** Copy file/dir. */                  copy:        function(src, dst)        { return _request.send(NV_FS_OPS.copy,        { src: src, dst: dst }); },
  /** Stat path. */                      stat:        function(path)            { return _request.send(NV_FS_OPS.stat,        { path: path }); },
  /** Read directory entries. */         readDir:     function(path)            { return _request.send(NV_FS_OPS.readDir,     { path: path }); },
  /** Make directory. */                 mkdir:       function(path, recursive) { return _request.send(NV_FS_OPS.mkdir,       { path: path, recursive: recursive }); },
  /** Remove directory. */               rmdir:       function(path, recursive) { return _request.send(NV_FS_OPS.rmdir,       { path: path, recursive: recursive }); },
  /** Start watching path. */            watch:       function(path, id)        { return _request.send(NV_FS_OPS.watch,       { path: path, id: id }); },
  /** Stop watching by id. */            unwatch:     function(id)              { return _request.send(NV_FS_OPS.unwatch,     { id: id }); },

  /** Subscribe to fs changed events. */ onChanged:   function(fn)              { return _events.on(NV_FS_EVENTS.changed, fn); }
};
