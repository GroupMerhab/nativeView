// SPDX-License-Identifier: Apache-2.0

import Darwin
import UIKit

/// Device info for the JS bridge (`device.getInfo`).
public enum DeviceOps {
    public static let namespace = "device"

    public static func register(app: NativeViewApp) {
        app.handle("device.getInfo", permissions: []) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                    let payload = try Self.buildInfoJson()
                    resolve(payload)
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }
    }

    private static func buildInfoJson() throws -> String {
        let screen = mainScreenBounds()
        UIDevice.current.isBatteryMonitoringEnabled = true
        let battery = UIDevice.current.batteryLevel
        let batteryLevel: Double = battery >= 0 ? Double(battery) : -1

        let machine = machineIdentifier()
        let model = marketingName(for: machine) ?? machine

        let obj: [String: Any] = [
            "platform": "ios",
            "version": UIDevice.current.systemVersion,
            "model": model,
            "manufacturer": "Apple",
            "screenWidth": Int(screen.width.rounded()),
            "screenHeight": Int(screen.height.rounded()),
            "batteryLevel": batteryLevel >= 0 ? batteryLevel : NSNull(),
            "isSimulator": isSimulatorBuild,
        ]
        guard let s = NVJsonWire.jsonString(obj) else {
            throw NVJsonWire.WireError.invalidJson
        }
        return s
    }

    private static var isSimulatorBuild: Bool {
        #if targetEnvironment(simulator)
            return true
        #else
            return false
        #endif
    }

    private static func mainScreenBounds() -> CGRect {
        let scenes = UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
        if let scene = scenes.first(where: { $0.activationState == .foregroundActive }) ?? scenes.first,
           let w = scene.windows.first(where: { $0.isKeyWindow }) ?? scene.windows.first
        {
            return w.bounds
        }
        return UIScreen.main.bounds
    }

    private static func machineIdentifier() -> String {
        var systemInfo = utsname()
        uname(&systemInfo)
        let mirror = Mirror(reflecting: systemInfo.machine)
        var bytes = [UInt8]()
        for child in mirror.children {
            guard let val = child.value as? Int8, val != 0 else { break }
            bytes.append(UInt8(bitPattern: val))
        }
        if bytes.isEmpty {
            return "unknown"
        }
        return String(bytes: bytes, encoding: .ascii) ?? "unknown"
    }

    /// Best-effort marketing names for common devices; unknown identifiers fall back to the hardware string.
    private static func marketingName(for machine: String) -> String? {
        let map: [String: String] = [
            "iPhone15,4": "iPhone 15",
            "iPhone15,5": "iPhone 15 Plus",
            "iPhone16,1": "iPhone 15 Pro",
            "iPhone16,2": "iPhone 15 Pro Max",
            "iPhone17,1": "iPhone 16 Pro",
            "iPhone17,2": "iPhone 16 Pro Max",
            "iPhone17,3": "iPhone 16",
            "iPhone17,4": "iPhone 16 Plus",
            "arm64": "Simulator",
            "x86_64": "Simulator",
        ]
        return map[machine]
    }
}
