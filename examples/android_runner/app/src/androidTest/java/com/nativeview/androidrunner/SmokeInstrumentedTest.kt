package com.nativeview.androidrunner

import android.os.SystemClock
import android.webkit.WebView
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SmokeInstrumentedTest {
  @Test
  fun webViewIpcRoundtrip() {
    val scenario = ActivityScenario.launch(MainActivity::class.java)

    try {
      val ready = CountDownLatch(1)
      scenario.onActivity { activity ->
        val root = activity.findViewById<android.view.ViewGroup>(android.R.id.content)
        val wv = findWebView(root)
        if (wv != null) {
          wv.postDelayed({ ready.countDown() }, 600)
        } else {
          ready.countDown()
        }
      }
      ready.await(2, TimeUnit.SECONDS)

      val ok = CountDownLatch(1)
      val deadlineMs = SystemClock.uptimeMillis() + 4000

      while (SystemClock.uptimeMillis() < deadlineMs && ok.count > 0) {
        scenario.onActivity { activity ->
          val root = activity.findViewById<android.view.ViewGroup>(android.R.id.content)
          val wv = findWebView(root)
          if (wv != null) {
            wv.evaluateJavascript("window.__nv && window.__nv.send('ping', JSON.stringify({ts: 1}));", null)
            wv.evaluateJavascript("document.getElementById('out') && document.getElementById('out').textContent;") { value ->
              if (value != null && value.contains("pong")) {
                ok.countDown()
              }
            }
          }
        }
        Thread.sleep(150)
      }

      assertTrue(ok.await(0, TimeUnit.MILLISECONDS))
    } finally {
      scenario.close()
    }
  }

  private fun findWebView(root: android.view.ViewGroup?): WebView? {
    if (root == null) return null
    for (i in 0 until root.childCount) {
      val v = root.getChildAt(i)
      if (v is WebView) return v
      if (v is android.view.ViewGroup) {
        val found = findWebView(v)
        if (found != null) return found
      }
    }
    return null
  }
}
