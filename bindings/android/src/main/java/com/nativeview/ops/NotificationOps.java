package com.nativeview.ops;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import com.nativeview.NativeViewApp;
import com.nativeview.bridge.BridgeArgs;
import org.json.JSONException;
import org.json.JSONObject;

/** Local notifications (system {@link NotificationManager}). */
public final class NotificationOps {

    private static final String CHANNEL_ID = "nativeview_default";

    private NotificationOps() {}

    public static void register(NativeViewApp app) {
        if (app == null) {
            return;
        }
        Context ctx = app.getAppContext();
        String[] postPerms =
                Build.VERSION.SDK_INT >= 33
                        ? new String[] {android.Manifest.permission.POST_NOTIFICATIONS}
                        : new String[] {};
        app.handle(
                "notification.show",
                postPerms,
                (argsJson, resolve, reject) -> {
                    try {
                        JSONObject a = BridgeArgs.requireObject(argsJson);
                        int id = a.optInt("id", 1);
                        String title = a.optString("title", "");
                        String body = a.optString("body", "");
                        ensureChannel(ctx);
                        android.app.Notification.Builder b;
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                            b = new android.app.Notification.Builder(ctx, CHANNEL_ID);
                        } else {
                            b = new android.app.Notification.Builder(ctx);
                        }
                        int smallIcon = ctx.getApplicationInfo().icon;
                        if (smallIcon != 0) {
                            b.setSmallIcon(smallIcon);
                        } else {
                            b.setSmallIcon(android.R.drawable.ic_dialog_info);
                        }
                        b.setContentTitle(title);
                        b.setContentText(body);
                        b.setAutoCancel(true);
                        Intent launch = ctx.getPackageManager().getLaunchIntentForPackage(ctx.getPackageName());
                        int piFlags = PendingIntent.FLAG_UPDATE_CURRENT;
                        if (Build.VERSION.SDK_INT >= 23) {
                            piFlags |= PendingIntent.FLAG_IMMUTABLE;
                        }
                        PendingIntent content =
                                PendingIntent.getActivity(ctx, id, launch != null ? launch : new Intent(), piFlags);
                        b.setContentIntent(content);
                        NotificationManager nm =
                                (NotificationManager) ctx.getSystemService(Context.NOTIFICATION_SERVICE);
                        if (nm == null) {
                            reject.reject("ERR_IO", "NotificationManager unavailable");
                            return;
                        }
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            if (ctx.checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS)
                                    != PackageManager.PERMISSION_GRANTED) {
                                reject.reject("ERR_PERMISSION", "POST_NOTIFICATIONS not granted");
                                return;
                            }
                        }
                        nm.notify(id, b.build());
                        resolve.resolve("{}");
                    } catch (JSONException ex) {
                        reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
                    }
                });
        app.handle(
                "notification.cancel",
                null,
                (argsJson, resolve, reject) -> {
                    try {
                        JSONObject a = BridgeArgs.requireObject(argsJson);
                        int id = a.optInt("id", 0);
                        NotificationManager nm =
                                (NotificationManager) ctx.getSystemService(Context.NOTIFICATION_SERVICE);
                        if (nm != null) {
                            nm.cancel(id);
                        }
                        resolve.resolve("{}");
                    } catch (JSONException ex) {
                        reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
                    }
                });
        app.handle(
                "notification.cancelAll",
                null,
                (argsJson, resolve, reject) -> {
                    NotificationManager nm =
                            (NotificationManager) ctx.getSystemService(Context.NOTIFICATION_SERVICE);
                    if (nm != null) {
                        nm.cancelAll();
                    }
                    resolve.resolve("{}");
                });
    }

    private static void ensureChannel(Context ctx) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return;
        }
        NotificationManager nm = (NotificationManager) ctx.getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm == null) {
            return;
        }
        NotificationChannel ch = nm.getNotificationChannel(CHANNEL_ID);
        if (ch != null) {
            return;
        }
        ch = new NotificationChannel(CHANNEL_ID, "NativeView", NotificationManager.IMPORTANCE_DEFAULT);
        nm.createNotificationChannel(ch);
    }
}
