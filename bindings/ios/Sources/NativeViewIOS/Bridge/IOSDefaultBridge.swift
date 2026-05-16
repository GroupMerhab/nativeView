// SPDX-License-Identifier: Apache-2.0

import Foundation

/// Registers default Swift bridge ops on ``NativeViewApp`` (parity with Android `AndroidBridgeRegistry`).
enum IOSDefaultBridge {
    static func registerAll(_ app: NativeViewApp) {
        DeviceOps.register(app: app)
        StorageOps.register(app: app)
        NetworkOps.register(app: app)
        NotificationOps.register(app: app)
        AppOps.register(app: app)
        CameraOps.register(app: app)
        LocationOps.register(app: app)
        FileOps.register(app: app)
        ClipboardOps.register(app: app)
        BiometricOps.register(app: app)
    }

    static func unregisterAll(_ app: NativeViewApp) {
        NetworkOps.unregister(app: app)
        LocationOps.unregister(app: app)
    }
}
