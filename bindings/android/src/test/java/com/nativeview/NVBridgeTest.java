package com.nativeview;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import android.app.Application;
import android.webkit.WebView;
import com.nativeview.bridge.NVBridge;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowLooper;

/**
 * {@link com.nativeview.bridge.NVBridge} posts {@link NativeViewApp#deliverWebMessage} onto the
 * WebView UI thread.
 */
@RunWith(RobolectricTestRunner.class)
public class NVBridgeTest {

    @Test
    public void post_nullJson_doesNotDeliver() {
        Application app = RuntimeEnvironment.getApplication();
        NativeViewApp nva = Mockito.mock(NativeViewApp.class);
        NativeViewWindow win = new NativeViewWindow(1L);
        WebView webView = new WebView(app);
        NVBridge bridge = new NVBridge(nva, win, webView);
        bridge.post(null);
        ShadowLooper.shadowMainLooper().idle();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Mockito.verifyNoInteractions(nva);
    }

    @Test
    public void post_deliversOnUiThread() {
        Application app = RuntimeEnvironment.getApplication();
        NativeViewApp nva = Mockito.mock(NativeViewApp.class);
        NativeViewWindow win = new NativeViewWindow(1L);
        WebView real = new WebView(app);
        WebView webView = Mockito.spy(real);
        doAnswer(
                        inv -> {
                            Runnable r = inv.getArgument(0);
                            r.run();
                            return null;
                        })
                .when(webView)
                .post(any(Runnable.class));
        NVBridge bridge = new NVBridge(nva, win, webView);
        String wire = "{\"e\":\"app.ping\",\"s\":2,\"d\":null}";
        bridge.post(wire);
        verify(nva).deliverWebMessage(eq(webView), eq(win), eq(wire));
    }

    @Test
    public void post_nullWindow_doesNotDeliver() {
        Application app = RuntimeEnvironment.getApplication();
        NativeViewApp nva = Mockito.mock(NativeViewApp.class);
        WebView webView = new WebView(app);
        NVBridge bridge = new NVBridge(nva, null, webView);
        bridge.post("{}");
        ShadowLooper.shadowMainLooper().idle();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Mockito.verifyNoInteractions(nva);
    }
}
