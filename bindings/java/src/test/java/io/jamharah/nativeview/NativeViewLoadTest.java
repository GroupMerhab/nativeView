package io.jamharah.nativeview;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;

import org.junit.Assume;
import org.junit.Test;

/** Desktop JNI smoke test; skipped when {@code libnativeview_jni} is not on {@code java.library.path}. */
public class NativeViewLoadTest {

    @Test
    public void versionStringWhenNativeLibraryLoads() {
        try {
            NativeView.loadLibrary();
        } catch (UnsatisfiedLinkError | IllegalStateException e) {
            Assume.assumeTrue("skip: " + e.getMessage(), false);
        }
        String v = NativeView.nvVersionString();
        assertNotNull(v);
        assertFalse(v.trim().isEmpty());
    }
}
