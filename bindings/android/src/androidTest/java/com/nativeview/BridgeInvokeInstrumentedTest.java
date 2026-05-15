package com.nativeview;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.webkit.WebView;
import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import com.nativeview.bridge.NVBridge;
import com.nativeview.jni.NativeViewJNI;
import com.nativeview.testutil.BridgeProbeActivity;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Full {@link NVBridge#post} → UI thread → {@link NativeViewApp#deliverWebMessage} path on a real
 * WebView (requires {@code libnativeview} for the host process).
 */
@RunWith(AndroidJUnit4.class)
public class BridgeInvokeInstrumentedTest {

    @Test
    public void nvBridge_postReachesDeliverOnUiThread() throws Exception {
        NativeViewJNI.ensureLoaded();
        final CountingNativeViewApp[] appHolder = new CountingNativeViewApp[1];
        try (ActivityScenario<BridgeProbeActivity> scenario = ActivityScenario.launch(BridgeProbeActivity.class)) {
            scenario.onActivity(
                    activity -> {
                        CountingNativeViewApp app = activity.getCountingApp();
                        appHolder[0] = app;
                        NativeViewWindow win = activity.getWindowHandle();
                        WebView webView = activity.getWebView();
                        assertNotNull(app);
                        assertNotNull(win);
                        assertNotNull(webView);
                        NVBridge bridge = new NVBridge(app, win, webView);
                        bridge.post("{\"e\":\"device.getInfo\",\"s\":1,\"d\":null}");
                    });
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
            assertNotNull(appHolder[0]);
            assertTrue(appHolder[0].awaitDeliver(15000));
        }
    }
}
