package com.nativeview.bridge;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.util.SparseArray;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Android runtime permission checks used before bridge handlers run. Missing permissions trigger
 * {@link Activity#requestPermissions(String[], int)}; the host activity must forward {@link
 * #onRequestPermissionsResult(int, String[], int[])} to {@link #deliverPermissionResult(int,
 * String[], int[])}.
 */
public final class NVPermissionManager {

    private static final Object LOCK = new Object();
    private static final AtomicInteger NEXT_CODE = new AtomicInteger(0x7100);
    private static final SparseArray<Pending> PENDING = new SparseArray<>();

    private NVPermissionManager() {}

    public static boolean hasPermission(Context context, String permission) {
        if (context == null || permission == null || permission.isEmpty()) {
            return false;
        }
        return context.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Runs {@code onAllGranted} immediately when every non-empty permission in {@code
     * permissions} is already granted. Otherwise requests missing permissions on {@code activity}
     * and completes on the UI thread when the user responds. If {@code activity} is null but
     * permissions are missing, {@code onDenied} runs with {@code ERR_PERMISSION}.
     */
    public static void checkThenRun(
            Context context,
            Activity activity,
            String[] permissions,
            Runnable onAllGranted,
            DenyCallback onDenied) {
        if (onAllGranted == null) {
            return;
        }
        Context ctx = context != null ? context.getApplicationContext() : null;
        List<String> need = collectMissing(ctx, permissions);
        if (need.isEmpty()) {
            onAllGranted.run();
            return;
        }
        if (activity == null) {
            if (onDenied != null) {
                onDenied.onDenied("ERR_PERMISSION", "activity required to request permissions");
            }
            return;
        }
        int code = allocateRequestCode();
        synchronized (LOCK) {
            PENDING.put(code, new Pending(onAllGranted, onDenied));
        }
        activity.requestPermissions(need.toArray(new String[0]), code);
    }

    /**
     * Call from {@link Activity#onRequestPermissionsResult(int, String[], int[])}. Returns {@code
     * true} when the request code belonged to this manager (and a pending callback was cleared).
     */
    public static boolean deliverPermissionResult(int requestCode, String[] permissions, int[] grantResults) {
        Pending p;
        synchronized (LOCK) {
            p = PENDING.get(requestCode);
            if (p == null) {
                return false;
            }
            PENDING.remove(requestCode);
        }
        if (permissions == null || grantResults == null || permissions.length != grantResults.length) {
            deny(p, "ERR_PERMISSION", "invalid permission result");
            return true;
        }
        for (int i = 0; i < grantResults.length; i++) {
            if (grantResults[i] != PackageManager.PERMISSION_GRANTED) {
                deny(p, "ERR_PERMISSION", "denied: " + permissions[i]);
                return true;
            }
        }
        Runnable g = p.granted;
        if (g != null) {
            g.run();
        }
        return true;
    }

    private static void deny(Pending p, String code, String message) {
        if (p.denied != null) {
            p.denied.onDenied(code, message);
        }
    }

    private static int allocateRequestCode() {
        synchronized (LOCK) {
            int c = NEXT_CODE.getAndIncrement();
            if (c < 0x7100 || c > 0x7ffe) {
                NEXT_CODE.set(0x7101);
                return 0x7100;
            }
            return c;
        }
    }

    private static List<String> collectMissing(Context ctx, String[] permissions) {
        List<String> need = new ArrayList<>();
        if (permissions == null || ctx == null) {
            return need;
        }
        for (String p : permissions) {
            if (p == null || p.isEmpty()) {
                continue;
            }
            if (!hasPermission(ctx, p)) {
                need.add(p);
            }
        }
        return need;
    }

    public interface DenyCallback {
        void onDenied(String code, String message);
    }

    private static final class Pending {
        final Runnable granted;
        final DenyCallback denied;

        Pending(Runnable granted, DenyCallback denied) {
            this.granted = granted;
            this.denied = denied;
        }
    }
}
