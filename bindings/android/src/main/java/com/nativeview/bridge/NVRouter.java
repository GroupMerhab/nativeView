package com.nativeview.bridge;

import com.nativeview.NVHandler;
import java.util.Collections;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Maps {@code namespace.method} wire keys (field {@code e}) to a {@link NVHandler} and optional
 * Android runtime permissions required before the handler runs.
 */
public final class NVRouter {

    /** Result of {@link #route(String)}: either a registered handler or {@link #NOT_AVAILABLE}. */
    public static final class RouteResult {

        /** No handler registered for the method. */
        public static final RouteResult NOT_AVAILABLE = new RouteResult(null, null);

        private final NVHandler handler;
        private final String[] permissions;

        private RouteResult(NVHandler handler, String[] permissions) {
            this.handler = handler;
            this.permissions = permissions;
        }

        public boolean isAvailable() {
            return handler != null;
        }

        public NVHandler getHandler() {
            return handler;
        }

        /** Never {@code null} (may be zero-length). */
        public String[] getPermissions() {
            return permissions != null ? permissions : new String[0];
        }
    }

    private final Map<String, RouteEntry> routes = new ConcurrentHashMap<>();

    /**
     * Registers a handler for {@code method} (for example {@code app.getVersion}). {@code
     * permissions} may be {@code null} or empty.
     */
    public void register(String method, String[] permissions, NVHandler handler) {
        if (method == null || method.isEmpty() || handler == null) {
            return;
        }
        String[] perms = permissions != null ? permissions.clone() : new String[0];
        routes.put(method, new RouteEntry(handler, perms));
    }

    public void unregister(String method) {
        if (method == null) {
            return;
        }
        routes.remove(method);
    }

    /**
     * Looks up a handler for the wire method key. Returns {@link RouteResult#NOT_AVAILABLE} when
     * missing.
     */
    public RouteResult route(String method) {
        if (method == null || method.isEmpty()) {
            return RouteResult.NOT_AVAILABLE;
        }
        RouteEntry e = routes.get(method);
        if (e == null) {
            return RouteResult.NOT_AVAILABLE;
        }
        return new RouteResult(e.handler, e.permissions);
    }

    /** Snapshot of registered method names (unmodifiable). */
    public Map<String, RouteResult> snapshot() {
        Map<String, RouteResult> copy = new ConcurrentHashMap<>();
        for (Map.Entry<String, RouteEntry> en : routes.entrySet()) {
            RouteEntry e = en.getValue();
            copy.put(en.getKey(), new RouteResult(e.handler, e.permissions));
        }
        return Collections.unmodifiableMap(copy);
    }

    private static final class RouteEntry {
        final NVHandler handler;
        final String[] permissions;

        RouteEntry(NVHandler handler, String[] permissions) {
            this.handler = handler;
            this.permissions = permissions;
        }
    }
}
