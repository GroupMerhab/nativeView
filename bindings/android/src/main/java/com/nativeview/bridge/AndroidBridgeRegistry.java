package com.nativeview.bridge;

import com.nativeview.NativeViewApp;
import com.nativeview.ops.AppOps;
import com.nativeview.ops.BiometricOps;
import com.nativeview.ops.CameraOps;
import com.nativeview.ops.ClipboardOps;
import com.nativeview.ops.DeviceOps;
import com.nativeview.ops.FileOps;
import com.nativeview.ops.LocationOps;
import com.nativeview.ops.NetworkOps;
import com.nativeview.ops.NotificationOps;
import com.nativeview.ops.StorageOps;

/**
 * Registers default Java-side bridge handlers for the Android embedding (device, storage,
 * network, …). Call {@link NativeViewApp#registerDefaultAndroidHandlers()} once after constructing
 * {@link NativeViewApp}.
 */
public final class AndroidBridgeRegistry {

    private AndroidBridgeRegistry() {}

    public static void registerAll(NativeViewApp app) {
        if (app == null) {
            return;
        }
        DeviceOps.register(app);
        StorageOps.register(app);
        NetworkOps.register(app);
        NotificationOps.register(app);
        AppOps.register(app);
        CameraOps.register(app);
        LocationOps.register(app);
        FileOps.register(app);
        ClipboardOps.register(app);
        BiometricOps.register(app);
    }

    public static void unregisterAll(NativeViewApp app) {
        if (app == null) {
            return;
        }
        NetworkOps.unregister(app);
        LocationOps.unregister(app);
    }
}
