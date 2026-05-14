package com.nativeview.androidrunner

object NativeviewBridge {
  init {
    System.loadLibrary("nv-platform-android")
  }

  external fun nativeSetHost(host: Host)
  external fun nativeGetInjectScript(): String
  external fun nativeCreateApp(): Long
  external fun nativeDestroyApp(appPtr: Long)
  external fun nativeCreateWindow(appPtr: Long, title: String, width: Int, height: Int): Long
  external fun nativeShowWindow(windowPtr: Long)
  external fun nativeLoadHtml(windowPtr: Long, html: String, baseUrl: String?)
  external fun nativeEvalJs(windowPtr: Long, js: String)
  external fun nativeInstallSmokeHandlers(windowPtr: Long)
  external fun nativeOnWebMessage(windowPtr: Long, wireJson: String)
  external fun nativeOnReady(windowPtr: Long)

  interface Host {
    fun dispatch(op: Int, windowPtr: Long, s1: String?, s2: String?)
  }
}

