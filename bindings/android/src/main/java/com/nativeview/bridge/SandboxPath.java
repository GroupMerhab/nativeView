package com.nativeview.bridge;

import java.io.File;
import java.io.IOException;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;

/**
 * Resolves a relative path under an app-private base directory, blocking traversal and absolute
 * paths.
 */
public final class SandboxPath {

    private SandboxPath() {}

    /**
     * Returns the canonical file for {@code relativePath} under {@code baseDir}.
     *
     * @throws SecurityException when the path is absolute, contains NUL, decodes to {@code ..}, or
     *     escapes the base after canonicalization
     * @throws IOException when the base directory cannot be canonicalized
     */
    public static File resolveUnderBase(File baseDir, String relativePath) throws IOException {
        if (baseDir == null) {
            throw new IOException("baseDir is null");
        }
        if (relativePath == null) {
            throw new SecurityException("path is null");
        }
        if (relativePath.indexOf('\u0000') >= 0) {
            throw new SecurityException("path contains NUL");
        }
        String dec;
        try {
            dec = URLDecoder.decode(relativePath, StandardCharsets.UTF_8.name());
        } catch (IllegalArgumentException ex) {
            throw new SecurityException("path decode failed");
        }
        if (dec.contains("..")) {
            throw new SecurityException("path must not contain '..'");
        }
        String norm = dec.replace('\\', '/');
        if (norm.startsWith("/") || norm.startsWith("//")) {
            throw new SecurityException("absolute path not allowed");
        }
        if (norm.length() >= 2 && norm.charAt(1) == ':') {
            throw new SecurityException("path not allowed");
        }
        File base = baseDir.getCanonicalFile();
        String withSep = norm.replace('/', File.separatorChar);
        File combined = new File(base, withSep);
        String canon = combined.getCanonicalPath();
        String baseCanon = base.getCanonicalPath();
        if (!canon.startsWith(baseCanon + File.separator) && !canon.equals(baseCanon)) {
            throw new SecurityException("path escapes sandbox");
        }
        return combined;
    }
}
