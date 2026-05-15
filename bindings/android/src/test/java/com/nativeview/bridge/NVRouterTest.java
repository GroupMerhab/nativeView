package com.nativeview.bridge;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import com.nativeview.NVHandler;
import java.util.Map;
import org.junit.Test;

public class NVRouterTest {

    @Test
    public void route_nullOrEmpty_notAvailable() {
        NVRouter r = new NVRouter();
        assertSame(NVRouter.RouteResult.NOT_AVAILABLE, r.route(null));
        assertSame(NVRouter.RouteResult.NOT_AVAILABLE, r.route(""));
    }

    @Test
    public void register_nullMethodOrHandler_ignored() {
        NVRouter r = new NVRouter();
        NVHandler h = (a, res, rej) -> {};
        r.register(null, null, h);
        r.register("", null, h);
        r.register("a.b", null, null);
        assertFalse(r.route("a.b").isAvailable());
    }

    @Test
    public void snapshot_reflectsRegistrations() {
        NVRouter r = new NVRouter();
        NVHandler h = (a, res, rej) -> {};
        r.register("a.one", new String[] {"p1"}, h);
        Map<String, NVRouter.RouteResult> snap = r.snapshot();
        assertTrue(snap.containsKey("a.one"));
        assertArrayEquals(new String[] {"p1"}, snap.get("a.one").getPermissions());
    }

    @Test
    public void route_missingReturnsNotAvailable() {
        NVRouter r = new NVRouter();
        assertSame(NVRouter.RouteResult.NOT_AVAILABLE, r.route("app.missing"));
        assertFalse(r.route("app.missing").isAvailable());
    }

    @Test
    public void register_routeReturnsHandlerAndPermissions() {
        NVRouter r = new NVRouter();
        NVHandler h = (args, resolve, reject) -> resolve.resolve("ok");
        r.register("app.ping", new String[] {"android.permission.CAMERA"}, h);
        NVRouter.RouteResult rr = r.route("app.ping");
        assertTrue(rr.isAvailable());
        assertEquals(h, rr.getHandler());
        assertArrayEquals(new String[] {"android.permission.CAMERA"}, rr.getPermissions());
    }

    @Test
    public void unregister_removesRoute() {
        NVRouter r = new NVRouter();
        NVHandler h = (a, res, rej) -> {};
        r.register("x.y", null, h);
        r.unregister("x.y");
        assertSame(NVRouter.RouteResult.NOT_AVAILABLE, r.route("x.y"));
    }

    @Test
    public void unregister_null_isNoOp() {
        NVRouter r = new NVRouter();
        NVHandler h = (a, res, rej) -> {};
        r.register("k.v", null, h);
        r.unregister(null);
        assertTrue(r.route("k.v").isAvailable());
    }
}
