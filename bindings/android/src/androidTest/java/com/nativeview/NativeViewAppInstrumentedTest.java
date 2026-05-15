package com.nativeview;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import com.nativeview.jni.NativeViewJNI;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * On-device checks: JNI load, default bridge registration, and idempotent {@link
 * NativeViewApp#destroy()}.
 */
@RunWith(AndroidJUnit4.class)
public class NativeViewAppInstrumentedTest {

    @Test
    public void nativeLibraryLoads() {
        NativeViewJNI.ensureLoaded();
    }

    @Test
    public void registerDefaultAndroidHandlers_registersCoreRoutes() {
        Context ctx = InstrumentationRegistry.getInstrumentation().getTargetContext();
        NativeViewApp app = new NativeViewApp(ctx);
        app.registerDefaultAndroidHandlers();
        assertTrue(app.getRouter().route("device.getInfo").isAvailable());
        assertTrue(app.getRouter().route("app.getVersion").isAvailable());
        assertTrue(app.getRouter().route("network.getStatus").isAvailable());
        assertTrue(app.getRouter().route("clipboard.readText").isAvailable());
        app.destroy();
    }

    @Test
    public void destroyTwice_isSafe() {
        Context ctx = InstrumentationRegistry.getInstrumentation().getTargetContext();
        NativeViewApp app = new NativeViewApp(ctx);
        app.destroy();
        app.destroy();
    }

    @Test
    public void allowedOrigins_includeFileSchemeAfterConstruction() {
        Context ctx = InstrumentationRegistry.getInstrumentation().getTargetContext();
        NativeViewApp app = new NativeViewApp(ctx);
        assertTrue(app.allowedOriginsSnapshot().contains("file://"));
        app.destroy();
    }
}
