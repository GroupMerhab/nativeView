// NativeView.tray API — system tray (depends on request.js, events.js, constants.js)
var NV_TRAY_OPS = {
  create:     "tray.create",
  destroy:    "tray.destroy",
  setIcon:    "tray.setIcon",
  setTooltip: "tray.setTooltip",
  setMenu:    "tray.setMenu"
};

var NV_TRAY_EVENTS = {
  clicked:    "tray.clicked",
  menuAction: "tray.menuAction"
};

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};
NativeView.tray = {
  /** Create a tray icon. */          create:     function(id, icon, tooltip) { return _request.send(NV_TRAY_OPS.create,     { id: id, icon: icon, tooltip: tooltip }); },
  /** Destroy a tray icon. */         destroy:    function(id)                 { return _request.fire(NV_TRAY_OPS.destroy,    { id: id }); },
  /** Update tray icon. */            setIcon:    function(id, icon)           { return _request.send(NV_TRAY_OPS.setIcon,    { id: id, icon: icon }); },
  /** Update tray tooltip. */         setTooltip: function(id, tooltip)        { return _request.send(NV_TRAY_OPS.setTooltip, { id: id, tooltip: tooltip }); },
  /** Update tray menu. */            setMenu:    function(id, items)          { return _request.send(NV_TRAY_OPS.setMenu,    { id: id, items: items }); },

  /** Subscribe to tray clicks. */    onClicked:    function(fn)               { return _events.on(NV_TRAY_EVENTS.clicked,    fn); },
  /** Subscribe to menu actions. */   onMenuAction: function(fn)               { return _events.on(NV_TRAY_EVENTS.menuAction, fn); }
};
