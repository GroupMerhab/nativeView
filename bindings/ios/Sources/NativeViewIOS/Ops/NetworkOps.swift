// SPDX-License-Identifier: Apache-2.0

import Foundation
import Network

/// Reachability via `NWPathMonitor` (`network.getStatus`, `network.onChange` → `network.change`).
public enum NetworkOps {
    public static let namespace = "network"

    private static let lock = NSLock()
    private static var monitor: NWPathMonitor?
    private static let monitorQueue = DispatchQueue(label: "nativeview.network.monitor")
    private static var subscribers: [ObjectIdentifier: [WeakWindowBox]] = [:]

    private final class WeakWindowBox {
        weak var window: NativeViewWindow?
        init(_ window: NativeViewWindow) { self.window = window }
    }

    public static func register(app: NativeViewApp) {
        let appId = ObjectIdentifier(app)

        app.handle("network.getStatus", permissions: []) { _, argsJson, resolve, reject in
            do {
                _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                let status = currentStatusSync()
                guard let out = NVJsonWire.jsonString(status) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                reject(NVRejectCode.invalidArgs, "malformed arguments")
            }
        }

        app.handle("network.onChange", permissions: []) { window, argsJson, resolve, reject in
            do {
                _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                lock.lock()
                var list = subscribers[appId] ?? []
                if !list.contains(where: { $0.window === window }) {
                    list.append(WeakWindowBox(window))
                }
                subscribers[appId] = list
                ensureMonitorUnlocked()
                lock.unlock()

                if let initial = statusJsonString() {
                    DispatchQueue.main.async {
                        window.emit(event: "network.change", json: initial)
                    }
                }

                let reply: [String: Any] = ["subscribed": true]
                guard let out = NVJsonWire.jsonString(reply) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                reject(NVRejectCode.invalidArgs, "malformed arguments")
            }
        }
    }

    public static func unregister(app: NativeViewApp) {
        let appId = ObjectIdentifier(app)
        lock.lock()
        subscribers[appId] = nil
        maybeStopMonitorUnlocked()
        lock.unlock()
    }

    private static func ensureMonitorUnlocked() {
        if monitor != nil { return }
        let m = NWPathMonitor()
        m.pathUpdateHandler = { _ in
            emitCurrentStatus()
        }
        m.start(queue: monitorQueue)
        monitor = m
    }

    private static func maybeStopMonitorUnlocked() {
        guard !hasAnySubscriberUnlocked() else { return }
        monitor?.cancel()
        monitor = nil
    }

    private static func hasAnySubscriberUnlocked() -> Bool {
        for (_, list) in subscribers {
            if list.contains(where: { $0.window != nil }) {
                return true
            }
        }
        return false
    }

    private static func emitCurrentStatus() {
        guard let json = statusJsonString() else { return }
        lock.lock()
        let snapshot = subscribers
        lock.unlock()

        DispatchQueue.main.async {
            for (_, list) in snapshot {
                for box in list {
                    guard let w = box.window else { continue }
                    w.emit(event: "network.change", json: json)
                }
            }
        }
    }

    private static func statusJsonString() -> String? {
        NVJsonWire.jsonString(currentStatusSync())
    }

    private static func currentStatusSync() -> [String: Any] {
        if let m = monitor {
            return status(for: m.currentPath)
        }
        let temp = NWPathMonitor()
        let path = temp.currentPath
        return status(for: path)
    }

    private static func status(for path: NWPath) -> [String: Any] {
        if path.status == .unsatisfied {
            return ["connected": false, "type": "none"]
        }
        if path.usesInterfaceType(.wifi) {
            return ["connected": true, "type": "wifi"]
        }
        if path.usesInterfaceType(.cellular) {
            return ["connected": true, "type": "cellular"]
        }
        if path.availableInterfaces.isEmpty {
            return ["connected": false, "type": "none"]
        }
        return ["connected": path.status == .satisfied, "type": "other"]
    }
}
