package com.nativeview;

import android.content.Context;
import android.webkit.WebView;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Test double that signals when {@link NativeViewApp#deliverWebMessage} runs. */
final class CountingNativeViewApp extends NativeViewApp {

    private final CountDownLatch delivered = new CountDownLatch(1);

    CountingNativeViewApp(Context ctx) {
        super(ctx);
    }

    @Override
    public void deliverWebMessage(WebView webView, NativeViewWindow window, String wireJson) {
        delivered.countDown();
        super.deliverWebMessage(webView, window, wireJson);
    }

    boolean awaitDeliver(long timeoutMs) throws InterruptedException {
        return delivered.await(timeoutMs, TimeUnit.MILLISECONDS);
    }
}
