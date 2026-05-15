package com.nativeview.bridge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public class ActivityResultRouterTest {

    @Test
    public void deliver_invokesCallbackAndConsumes() {
        AtomicInteger hits = new AtomicInteger();
        int code =
                ActivityResultRouter.submit(
                        (resultCode, data) -> {
                            hits.incrementAndGet();
                            assertEquals(200, resultCode);
                            assertEquals("x", data.getStringExtra("k"));
                        });
        Intent i = new Intent();
        i.putExtra("k", "x");
        assertTrue(ActivityResultRouter.deliver(code, 200, i));
        assertEquals(1, hits.get());
        assertFalse(ActivityResultRouter.deliver(code, 200, i));
    }

    @Test
    public void deliver_unknownCodeReturnsFalse() {
        assertFalse(ActivityResultRouter.deliver(99999, 0, null));
    }
}
