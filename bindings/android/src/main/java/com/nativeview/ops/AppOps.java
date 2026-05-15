package com.nativeview.ops;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import com.nativeview.NativeViewApp;
import org.json.JSONException;
import org.json.JSONObject;

/** Package / activity helpers. */
public final class AppOps {

    private AppOps() {}

    public static void register(NativeViewApp app) {
        if (app == null) {
            return;
        }
        Context ctx = app.getAppContext();
        app.handle(
                "app.getVersion",
                null,
                (argsJson, resolve, reject) -> {
                    try {
                        PackageManager pm = ctx.getPackageManager();
                        String pkg = ctx.getPackageName();
                        PackageInfo pi = pm.getPackageInfo(pkg, 0);
                        JSONObject o = new JSONObject();
                        o.put("versionName", pi.versionName != null ? pi.versionName : "");
                        long code =
                                android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.P
                                        ? pi.getLongVersionCode()
                                        : pi.versionCode;
                        o.put("versionCode", code);
                        resolve.resolve(o.toString());
                    } catch (PackageManager.NameNotFoundException | JSONException ex) {
                        reject.reject("ERR_IO", ex.getMessage() != null ? ex.getMessage() : "package error");
                    }
                });
        app.handle(
                "app.getPlatform",
                null,
                (argsJson, resolve, reject) -> {
                    try {
                        JSONObject o = new JSONObject();
                        o.put("platform", "android");
                        resolve.resolve(o.toString());
                    } catch (JSONException ex) {
                        reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
                    }
                });
        app.handle(
                "app.getInfo",
                null,
                (argsJson, resolve, reject) -> {
                    try {
                        ApplicationInfo ai = ctx.getApplicationInfo();
                        boolean debug = (ai.flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
                        JSONObject o = new JSONObject();
                        o.put("packageName", ctx.getPackageName());
                        o.put("buildType", debug ? "debug" : "release");
                        resolve.resolve(o.toString());
                    } catch (JSONException ex) {
                        reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
                    }
                });
        app.handle(
                "app.exit",
                null,
                (argsJson, resolve, reject) -> {
                    Activity act = app.getHostActivity();
                    if (act == null) {
                        reject.reject("ERR_ACTIVITY", "setPermissionActivity / host activity required");
                        return;
                    }
                    resolve.resolve("{}");
                    act.finish();
                });
    }
}
