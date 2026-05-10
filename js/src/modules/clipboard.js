// NativeView.clipboard API — clipboard operations (depends on request.js, constants.js)
var NV_CLIPBOARD_OPS = {
  readText:   "clipboard.readText",
  writeText:  "clipboard.writeText",
  readImage:  "clipboard.readImage",
  writeImage: "clipboard.writeImage",
  clear:      "clipboard.clear",
  hasText:    "clipboard.hasText",
  hasImage:   "clipboard.hasImage"
};

var NativeView = typeof NativeView !== "undefined" ? NativeView : {};
NativeView.clipboard = {
  /** Read text from clipboard. */     readText:   function()       { return _request.send(NV_CLIPBOARD_OPS.readText,   {}); },
  /** Write text to clipboard. */      writeText:  function(text)    { return _request.send(NV_CLIPBOARD_OPS.writeText,  { text: text }); },
  /** Read image from clipboard ({ width, height, format: 'png', data: base64 }). */
  readImage:  function() { return _request.send(NV_CLIPBOARD_OPS.readImage, {}); },
  /** Write PNG (base64). Pass a string or { format?: 'png', data: base64 }. */
  writeImage: function(b64OrOpts) {
    if (b64OrOpts && typeof b64OrOpts === 'object') {
      var d = b64OrOpts.data || b64OrOpts.bytes || b64OrOpts.b64;
      if (d) {
        return _request.send(NV_CLIPBOARD_OPS.writeImage, {
          format: b64OrOpts.format || 'png',
          data: d
        });
      }
    }
    return _request.send(NV_CLIPBOARD_OPS.writeImage, { format: 'png', data: b64OrOpts });
  },
  /** Clear clipboard (fire-and-forget). */  clear:      function()       { return _request.fire(NV_CLIPBOARD_OPS.clear,     {}); },
  /** Check if clipboard has text. */  hasText:    function()       { return _request.send(NV_CLIPBOARD_OPS.hasText,   {}); },
  /** Check if clipboard has image. */ hasImage:   function()       { return _request.send(NV_CLIPBOARD_OPS.hasImage,  {}); }
};
