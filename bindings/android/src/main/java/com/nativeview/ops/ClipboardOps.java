package com.nativeview.ops;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import com.nativeview.NativeViewApp;
import com.nativeview.bridge.BridgeArgs;
import org.json.JSONException;
import org.json.JSONObject;

/** System clipboard text. */
public final class ClipboardOps {

    private ClipboardOps() {}

    public static void register(NativeViewApp app) {
        if (app == null) {
            return;
        }
        Context ctx = app.getAppContext();
        app.handle(
                "clipboard.readText",
                null,
                (argsJson, resolve, reject) -> {
                    try {
                        ClipboardManager cm =
                                (ClipboardManager) ctx.getSystemService(Context.CLIPBOARD_SERVICE);
                        String text = "";
                        if (cm != null && cm.hasPrimaryClip() && cm.getPrimaryClip() != null) {
                            ClipData clip = cm.getPrimaryClip();
                            if (clip != null && clip.getItemCount() > 0) {
                                CharSequence cs = clip.getItemAt(0).getText();
                                text = cs != null ? cs.toString() : "";
                            }
                        }
                        JSONObject o = new JSONObject();
                        o.put("text", text);
                        resolve.resolve(o.toString());
                    } catch (JSONException ex) {
                        reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
                    }
                });
        app.handle(
                "clipboard.writeText",
                null,
                (argsJson, resolve, reject) -> {
                    try {
                        JSONObject a = BridgeArgs.requireObject(argsJson);
                        String text = a.optString("text", "");
                        ClipboardManager cm =
                                (ClipboardManager) ctx.getSystemService(Context.CLIPBOARD_SERVICE);
                        if (cm != null) {
                            cm.setPrimaryClip(ClipData.newPlainText("nativeview", text));
                        }
                        resolve.resolve("{}");
                    } catch (JSONException ex) {
                        reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
                    }
                });
        app.handle(
                "clipboard.clear",
                null,
                (argsJson, resolve, reject) -> {
                    ClipboardManager cm = (ClipboardManager) ctx.getSystemService(Context.CLIPBOARD_SERVICE);
                    if (cm != null) {
                        cm.clearPrimaryClip();
                    }
                    resolve.resolve("{}");
                });
    }
}
