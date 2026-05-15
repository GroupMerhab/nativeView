package com.nativeview.bridge;

import android.net.Uri;
import java.util.Locale;
import java.util.Set;

/** Normalizes WebView document origins for {@link com.nativeview.NativeViewApp#allow_origin}. */
public final class BridgeOrigin {

    private BridgeOrigin() {}

    /**
     * Normalizes an {@code allow_origin} entry to scheme + host (+ non-default port). Returns
     * {@code null} when the string cannot be interpreted as an origin.
     */
    public static String normalizeAllowedOrigin(String entry) {
        if (entry == null) {
            return null;
        }
        String s = entry.trim();
        if (s.isEmpty()) {
            return null;
        }
        Uri u = Uri.parse(s);
        String scheme = u.getScheme();
        if (scheme == null) {
            return null;
        }
        String sl = scheme.toLowerCase(Locale.US);
        if ("file".equals(sl)) {
            return "file://";
        }
        String host = u.getHost();
        if (host == null || host.isEmpty()) {
            return null;
        }
        host = host.toLowerCase(Locale.US);
        int port = u.getPort();
        port = normalizeDefaultPort(sl, port);
        String auth = port > 0 ? host + ":" + port : host;
        return sl + "://" + auth;
    }

    /** Origin string for {@code pageUrl}, or {@code null} when it cannot be derived. */
    public static String pageOrigin(String pageUrl) {
        if (pageUrl == null) {
            return null;
        }
        String trimmed = pageUrl.trim();
        if (trimmed.isEmpty()) {
            return null;
        }
        Uri u = Uri.parse(trimmed);
        String scheme = u.getScheme();
        if (scheme == null) {
            return null;
        }
        if ("file".equalsIgnoreCase(scheme)) {
            return "file://";
        }
        String host = u.getHost();
        if (host == null || host.isEmpty()) {
            return null;
        }
        host = host.toLowerCase(Locale.US);
        String sl = scheme.toLowerCase(Locale.US);
        int port = normalizeDefaultPort(sl, u.getPort());
        String auth = port > 0 ? host + ":" + port : host;
        return sl + "://" + auth;
    }

    /**
     * Returns {@code true} when {@code pageUrl} has a non-null origin that appears in {@code
     * normalizedAllowed} (each element must already be {@linkplain #normalizeAllowedOrigin
     * normalized}).
     */
    public static boolean isAllowed(String pageUrl, Set<String> normalizedAllowed) {
        if (normalizedAllowed == null || normalizedAllowed.isEmpty()) {
            return false;
        }
        String origin = pageOrigin(pageUrl);
        if (origin == null) {
            return false;
        }
        return normalizedAllowed.contains(origin);
    }

    private static int normalizeDefaultPort(String schemeLower, int port) {
        if ("https".equals(schemeLower) && port == 443) {
            return -1;
        }
        if ("http".equals(schemeLower) && port == 80) {
            return -1;
        }
        if ("wss".equals(schemeLower) && port == 443) {
            return -1;
        }
        if ("ws".equals(schemeLower) && port == 80) {
            return -1;
        }
        return port;
    }
}
