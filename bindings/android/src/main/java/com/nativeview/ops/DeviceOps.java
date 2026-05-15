package com.nativeview.ops;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.os.BatteryManager;
import android.os.Build;
import com.nativeview.NativeViewApp;
import org.json.JSONException;
import org.json.JSONObject;

/** Device information (screen, battery, build identifiers). */
public final class DeviceOps {

    private DeviceOps() {}

    public static void register(NativeViewApp app) {
        if (app == null) {
            return;
        }
        app.handle(
                "device.getInfo",
                null,
                (argsJson, resolve, reject) -> {
                    try {
                        Context ctx = app.getAppContext();
                        JSONObject o = new JSONObject();
                        o.put("platform", "android");
                        o.put("version", Build.VERSION.RELEASE != null ? Build.VERSION.RELEASE : "");
                        o.put("manufacturer", Build.MANUFACTURER != null ? Build.MANUFACTURER : "");
                        o.put("model", Build.MODEL != null ? Build.MODEL : "");
                        Resources r = ctx.getResources();
                        android.util.DisplayMetrics dm = r.getDisplayMetrics();
                        o.put("screenWidth", dm.widthPixels);
                        o.put("screenHeight", dm.heightPixels);
                        o.put("batteryLevel", readBatteryFraction(ctx));
                        resolve.resolve(o.toString());
                    } catch (JSONException ex) {
                        reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
                    }
                });
    }

    private static Double readBatteryFraction(Context ctx) {
        if (ctx == null) {
            return null;
        }
        IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent battery = ctx.registerReceiver(null, filter);
        if (battery == null) {
            return null;
        }
        int level = battery.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int scale = battery.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (level < 0 || scale <= 0) {
            return null;
        }
        return level / (double) scale;
    }
}
