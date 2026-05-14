package com.nativeview.androidrunner

import android.annotation.SuppressLint
import android.app.Activity
import android.os.Bundle
import android.webkit.JavascriptInterface
import android.webkit.WebView
import android.webkit.WebViewClient

class MainActivity : Activity(), NativeviewBridge.Host {
  private lateinit var webView: WebView
  private var appPtr: Long = 0
  private var windowPtr: Long = 0

  private companion object {
    private const val OP_WINDOW_CREATE = 1
    private const val OP_WINDOW_DESTROY = 2
    private const val OP_WINDOW_SHOW = 3
    private const val OP_WINDOW_HIDE = 4
    private const val OP_WINDOW_LOAD_URL = 5
    private const val OP_WINDOW_LOAD_HTML = 6
    private const val OP_WINDOW_EVAL_JS = 7
  }

  private class JsBridge(private val onMessage: (String) -> Unit) {
    @JavascriptInterface
    fun postMessage(msg: String) {
      onMessage(msg)
    }
  }

  @SuppressLint("SetJavaScriptEnabled")
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)

    webView = WebView(this)
    setContentView(webView)

    webView.settings.javaScriptEnabled = true
    webView.settings.domStorageEnabled = true

    webView.addJavascriptInterface(JsBridge { msg ->
      if (windowPtr != 0L) {
        NativeviewBridge.nativeOnWebMessage(windowPtr, msg)
      }
    }, "nvBridge")

    webView.webViewClient = object : WebViewClient() {
      override fun onPageFinished(view: WebView, url: String) {
        if (windowPtr != 0L) {
          NativeviewBridge.nativeOnReady(windowPtr)
        }
      }
    }

    NativeviewBridge.nativeSetHost(this)

    appPtr = NativeviewBridge.nativeCreateApp()
    windowPtr = NativeviewBridge.nativeCreateWindow(appPtr, "nativeview android", 360, 640)
    NativeviewBridge.nativeInstallSmokeHandlers(windowPtr)

    val injected = NativeviewBridge.nativeGetInjectScript()
      .replace("/*{NV_POST}*/", "window.nvBridge.postMessage")

    val html = """
      <!doctype html>
      <html>
        <head>
          <meta charset="utf-8" />
          <meta name="viewport" content="width=device-width, initial-scale=1" />
          <title>nativeview android runner</title>
          <style>
            body{font-family:sans-serif;margin:0;padding:24px;background:#111;color:#eee}
            button{padding:10px 14px;font-size:16px}
          </style>
        </head>
        <body>
          <h1>nativeview android runner</h1>
          <p>If you can see this and "pong" appears, WebView + JNI + IPC are working.</p>
          <button onclick="window.__nv.send('ping', JSON.stringify({ts: Date.now()}))">Ping native</button>
          <pre id="out"></pre>
          <script>${injected}</script>
          <script>
            var out=document.getElementById('out');
            window.__nv.on('pong', function(d){ out.textContent = 'pong: '+JSON.stringify(d); });
          </script>
        </body>
      </html>
    """.trimIndent()

    webView.loadDataWithBaseURL("about:blank", html, "text/html", "utf-8", null)
    NativeviewBridge.nativeShowWindow(windowPtr)
  }

  override fun onDestroy() {
    if (windowPtr != 0L) {
      windowPtr = 0
    }
    if (appPtr != 0L) {
      NativeviewBridge.nativeDestroyApp(appPtr)
      appPtr = 0
    }
    super.onDestroy()
  }

  override fun dispatch(op: Int, windowPtr: Long, s1: String?, s2: String?) {
    runOnUiThread {
      when (op) {
        OP_WINDOW_CREATE -> Unit
        OP_WINDOW_DESTROY -> Unit
        OP_WINDOW_SHOW -> Unit
        OP_WINDOW_HIDE -> Unit
        OP_WINDOW_LOAD_URL -> {
          if (s1 != null) webView.loadUrl(s1)
        }
        OP_WINDOW_LOAD_HTML -> {
          val html = s1 ?: ""
          val base = s2 ?: "about:blank"
          webView.loadDataWithBaseURL(base, html, "text/html", "utf-8", null)
        }
        OP_WINDOW_EVAL_JS -> {
          val js = s1 ?: ""
          webView.evaluateJavascript(js, null)
        }
      }
    }
  }
}

