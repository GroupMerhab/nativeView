package com.nativeview.ops;

import android.content.Context;
import android.util.Base64;
import com.nativeview.NativeViewApp;
import com.nativeview.bridge.BridgeArgs;
import com.nativeview.bridge.SandboxPath;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * File helpers under app-private directories ({@code filesDir}, {@code cacheDir}, {@code
 * externalFilesDir}). Relative paths are rooted at {@code filesDir}. Optional {@code root} in args:
 * {@code "files"} (default), {@code "cache"}, or {@code "externalFiles"}.
 */
public final class FileOps {

    private FileOps() {}

    public static void register(NativeViewApp app) {
        if (app == null) {
            return;
        }
        Context ctx = app.getAppContext();
        app.handle("fs.readFile", null, (argsJson, resolve, reject) -> readFile(ctx, argsJson, resolve, reject));
        app.handle("fs.writeFile", null, (argsJson, resolve, reject) -> writeFile(ctx, argsJson, resolve, reject));
        app.handle("fs.deleteFile", null, (argsJson, resolve, reject) -> deleteFile(ctx, argsJson, resolve, reject));
        app.handle("fs.exists", null, (argsJson, resolve, reject) -> exists(ctx, argsJson, resolve, reject));
        app.handle("fs.listDir", null, (argsJson, resolve, reject) -> listDir(ctx, argsJson, resolve, reject));
    }

    private static void readFile(
            Context ctx, String argsJson, com.nativeview.ResolveCallback resolve, com.nativeview.RejectCallback reject) {
        try {
            JSONObject a = BridgeArgs.requireObject(argsJson);
            String path = a.optString("path", "");
            if (path.isEmpty()) {
                reject.reject("ERR_INVALID_ARG", "path required");
                return;
            }
            String encoding = a.optString("encoding", "text");
            File f = resolvePath(ctx, path, a.optString("root", "files"), reject);
            if (f == null) {
                return;
            }
            if (!f.isFile()) {
                reject.reject("ERR_IO", "not a file");
                return;
            }
            byte[] bytes = readAllBytes(f);
            JSONObject o = new JSONObject();
            if ("base64".equalsIgnoreCase(encoding)) {
                o.put("data", Base64.encodeToString(bytes, Base64.NO_WRAP));
            } else {
                o.put("data", new String(bytes, StandardCharsets.UTF_8));
            }
            resolve.resolve(o.toString());
        } catch (JSONException ex) {
            reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
        } catch (IOException ex) {
            reject.reject("ERR_IO", ex.getMessage() != null ? ex.getMessage() : "io error");
        }
    }

    private static void writeFile(
            Context ctx, String argsJson, com.nativeview.ResolveCallback resolve, com.nativeview.RejectCallback reject) {
        try {
            JSONObject a = BridgeArgs.requireObject(argsJson);
            String path = a.optString("path", "");
            if (path.isEmpty()) {
                reject.reject("ERR_INVALID_ARG", "path required");
                return;
            }
            String encoding = a.optString("encoding", "text");
            File f = resolvePath(ctx, path, a.optString("root", "files"), reject);
            if (f == null) {
                return;
            }
            File parent = f.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs()) {
                reject.reject("ERR_IO", "cannot create parent dirs");
                return;
            }
            byte[] bytes;
            if ("base64".equalsIgnoreCase(encoding)) {
                String b64 = a.optString("data", "");
                bytes = Base64.decode(b64, Base64.DEFAULT);
            } else {
                bytes = a.optString("data", "").getBytes(StandardCharsets.UTF_8);
            }
            try (FileOutputStream out = new FileOutputStream(f)) {
                out.write(bytes);
            }
            resolve.resolve("{}");
        } catch (JSONException ex) {
            reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
        } catch (IOException | IllegalArgumentException ex) {
            reject.reject("ERR_IO", ex.getMessage() != null ? ex.getMessage() : "io error");
        }
    }

    private static void deleteFile(
            Context ctx, String argsJson, com.nativeview.ResolveCallback resolve, com.nativeview.RejectCallback reject) {
        try {
            JSONObject a = BridgeArgs.requireObject(argsJson);
            String path = a.optString("path", "");
            if (path.isEmpty()) {
                reject.reject("ERR_INVALID_ARG", "path required");
                return;
            }
            File f = resolvePath(ctx, path, a.optString("root", "files"), reject);
            if (f == null) {
                return;
            }
            if (!f.exists()) {
                resolve.resolve("{}");
                return;
            }
            if (!f.delete()) {
                reject.reject("ERR_IO", "delete failed");
                return;
            }
            resolve.resolve("{}");
        } catch (JSONException ex) {
            reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
        }
    }

    private static void exists(
            Context ctx, String argsJson, com.nativeview.ResolveCallback resolve, com.nativeview.RejectCallback reject) {
        try {
            JSONObject a = BridgeArgs.requireObject(argsJson);
            String path = a.optString("path", "");
            if (path.isEmpty()) {
                reject.reject("ERR_INVALID_ARG", "path required");
                return;
            }
            File f = resolvePath(ctx, path, a.optString("root", "files"), reject);
            if (f == null) {
                return;
            }
            JSONObject o = new JSONObject();
            o.put("exists", f.exists());
            resolve.resolve(o.toString());
        } catch (JSONException ex) {
            reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
        }
    }

    private static void listDir(
            Context ctx, String argsJson, com.nativeview.ResolveCallback resolve, com.nativeview.RejectCallback reject) {
        try {
            JSONObject a = BridgeArgs.requireObject(argsJson);
            String path = a.optString("path", "");
            File dir = resolvePath(ctx, path, a.optString("root", "files"), reject);
            if (dir == null) {
                return;
            }
            if (!dir.isDirectory()) {
                reject.reject("ERR_IO", "not a directory");
                return;
            }
            String[] names = dir.list();
            if (names == null) {
                names = new String[0];
            }
            Arrays.sort(names);
            JSONArray arr = new JSONArray();
            for (String n : names) {
                arr.put(n);
            }
            JSONObject o = new JSONObject();
            o.put("names", arr);
            resolve.resolve(o.toString());
        } catch (JSONException ex) {
            reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
        }
    }

    private static File resolvePath(Context ctx, String path, String rootName, com.nativeview.RejectCallback reject) {
        File base = baseDir(ctx, rootName);
        if (base == null) {
            reject.reject("ERR_INVALID_ARG", "unknown root");
            return null;
        }
        try {
            return SandboxPath.resolveUnderBase(base, path);
        } catch (SecurityException ex) {
            reject.reject("ERR_INVALID_ARG", ex.getMessage() != null ? ex.getMessage() : "invalid path");
            return null;
        } catch (IOException ex) {
            reject.reject("ERR_IO", ex.getMessage() != null ? ex.getMessage() : "io error");
            return null;
        }
    }

    private static File baseDir(Context ctx, String rootName) {
        if (rootName == null || rootName.isEmpty() || "files".equals(rootName)) {
            return ctx.getFilesDir();
        }
        if ("cache".equals(rootName)) {
            return ctx.getCacheDir();
        }
        if ("externalFiles".equals(rootName)) {
            return ctx.getExternalFilesDir(null);
        }
        return null;
    }

    private static byte[] readAllBytes(File f) throws IOException {
        try (FileInputStream in = new FileInputStream(f);
                java.io.ByteArrayOutputStream buf = new java.io.ByteArrayOutputStream()) {
            byte[] tmp = new byte[8192];
            int n;
            while ((n = in.read(tmp)) != -1) {
                buf.write(tmp, 0, n);
            }
            return buf.toByteArray();
        }
    }
}
