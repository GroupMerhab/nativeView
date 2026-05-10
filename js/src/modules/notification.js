// NativeView.notification API — notifications (depends on request.js, events.js, constants.js)
var NV_NOTIFICATION_OPS = {
  show:  "notification.show",
  close: "notification.close"
};

var NV_NOTIFICATION_EVENTS = {
  clicked: "notification.clicked",
  closed:  "notification.closed"
};

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};
NativeView.notification = {
  /** Show a notification. */        show:     function(id, title, body, icon) { return _request.send(NV_NOTIFICATION_OPS.show,  { id: id, title: title, body: body, icon: icon }); },
  /** Close a notification. */       close:    function(id)                     { return _request.fire(NV_NOTIFICATION_OPS.close, { id: id }); },
  /** Subscribe to click events. */  onClicked:function(fn)                     { return _events.on(NV_NOTIFICATION_EVENTS.clicked, fn); },
  /** Subscribe to closed events. */ onClosed: function(fn)                     { return _events.on(NV_NOTIFICATION_EVENTS.closed,  fn); }
};
