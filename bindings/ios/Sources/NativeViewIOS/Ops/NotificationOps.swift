// SPDX-License-Identifier: Apache-2.0

import Foundation
import UserNotifications

/// Local notifications (`notification.*`) via `UNUserNotificationCenter`.
public enum NotificationOps {
    public static let namespace = "notification"

    public static func register(app: NativeViewApp) {
        app.handle("notification.show", permissions: [.notifications]) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    let a = try NVJsonWire.decodeObject(argsJson: argsJson)
                    let title = NVJsonWire.string(a, key: "title")
                    let body = NVJsonWire.string(a, key: "body")
                    let idString = notificationId(from: a["id"])

                    let content = UNMutableNotificationContent()
                    content.title = title
                    content.body = body
                    content.sound = .default

                    let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 0.5, repeats: false)
                    let request = UNNotificationRequest(identifier: idString, content: content, trigger: trigger)
                    UNUserNotificationCenter.current().add(request) { error in
                        if let error {
                            reject("ERR_IO", error.localizedDescription)
                            return
                        }
                        guard let out = NVJsonWire.jsonString([:]) else {
                            reject("ERR_JSON", "encode failed")
                            return
                        }
                        resolve(out)
                    }
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }

        app.handle("notification.cancel", permissions: []) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    let a = try NVJsonWire.decodeObject(argsJson: argsJson)
                    let idString = notificationId(from: a["id"])
                    UNUserNotificationCenter.current().removePendingNotificationRequests(withIdentifiers: [idString])
                    guard let out = NVJsonWire.jsonString([:]) else {
                        reject("ERR_JSON", "encode failed")
                        return
                    }
                    resolve(out)
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }

        app.handle("notification.cancelAll", permissions: []) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                    UNUserNotificationCenter.current().removeAllPendingNotificationRequests()
                    guard let out = NVJsonWire.jsonString([:]) else {
                        reject("ERR_JSON", "encode failed")
                        return
                    }
                    resolve(out)
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }
    }

    private static func notificationId(from value: Any?) -> String {
        if let s = value as? String, !s.isEmpty { return s }
        if let n = value as? NSNumber { return n.stringValue }
        return "1"
    }
}
