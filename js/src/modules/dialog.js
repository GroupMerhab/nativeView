// NativeView.dialog API — dialog operations (depends on request.js, constants.js)
var NV_DIALOG_OPS = {
  openFile:    "dialog.openFile",
  saveFile:    "dialog.saveFile",
  openFolder:  "dialog.openFolder",
  message:     "dialog.message",
  confirm:     "dialog.confirm"
};

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};
NativeView.dialog = {
  /** Open file picker. */      openFile:   function(title, filters, multiple)   { return _request.send(NV_DIALOG_OPS.openFile,   { title: title, filters: filters, multiple: multiple }); },
  /** Save file dialog. */      saveFile:   function(title, filters, defaultName){ return _request.send(NV_DIALOG_OPS.saveFile,   { title: title, filters: filters, defaultName: defaultName }); },
  /** Open folder picker. */    openFolder: function(title)                       { return _request.send(NV_DIALOG_OPS.openFolder, { title: title }); },
  /** Message dialog. */        message:    function(title, body, type, buttons)  { return _request.send(NV_DIALOG_OPS.message,    { title: title, body: body, type: type, buttons: buttons }); },
  /** Confirm dialog. */        confirm:    function(title, body)                 { return _request.send(NV_DIALOG_OPS.confirm,    { title: title, body: body }); }
};
