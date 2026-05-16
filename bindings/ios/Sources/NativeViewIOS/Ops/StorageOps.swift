// SPDX-License-Identifier: Apache-2.0

import Foundation

/// `UserDefaults.standard` key/value bridge (`storage.*`).
public enum StorageOps {
    public static let namespace = "storage"

    public static func register(app: NativeViewApp) {
        app.handle("storage.set", permissions: []) { _, argsJson, resolve, reject in
            do {
                let a = try NVJsonWire.decodeObject(argsJson: argsJson)
                let key = NVJsonWire.string(a, key: "key")
                guard !key.isEmpty else {
                    reject("ERR_INVALID_ARG", "key required")
                    return
                }
                try validateKeySegment(key)
                if a.keys.contains("value"), a["value"] is NSNull || a["value"] == nil {
                    UserDefaults.standard.removeObject(forKey: key)
                } else if let v = a["value"] {
                    Self.setUserDefaultsValue(v, forKey: key)
                } else {
                    UserDefaults.standard.removeObject(forKey: key)
                }
                guard let out = NVJsonWire.jsonString([:]) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                reject(NVRejectCode.invalidArgs, "malformed arguments")
            }
        }

        app.handle("storage.get", permissions: []) { _, argsJson, resolve, reject in
            do {
                let a = try NVJsonWire.decodeObject(argsJson: argsJson)
                let key = NVJsonWire.string(a, key: "key")
                guard !key.isEmpty else {
                    reject("ERR_INVALID_ARG", "key required")
                    return
                }
                try validateKeySegment(key)
                let type = NVJsonWire.string(a, key: "type", default: "string")
                let d = UserDefaults.standard.dictionaryRepresentation()
                let obj: [String: Any]
                if d[key] == nil {
                    obj = ["value": NSNull()]
                } else {
                    obj = ["value": Self.getTyped(key: key, type: type)]
                }
                guard let out = NVJsonWire.jsonString(obj) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                reject(NVRejectCode.invalidArgs, "malformed arguments")
            }
        }

        app.handle("storage.remove", permissions: []) { _, argsJson, resolve, reject in
            do {
                let a = try NVJsonWire.decodeObject(argsJson: argsJson)
                let key = NVJsonWire.string(a, key: "key")
                guard !key.isEmpty else {
                    reject("ERR_INVALID_ARG", "key required")
                    return
                }
                try validateKeySegment(key)
                UserDefaults.standard.removeObject(forKey: key)
                guard let out = NVJsonWire.jsonString([:]) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                reject(NVRejectCode.invalidArgs, "malformed arguments")
            }
        }

        app.handle("storage.clear", permissions: []) { _, argsJson, resolve, reject in
            do {
                _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                guard let bundleId = Bundle.main.bundleIdentifier else {
                    reject("ERR_IO", "bundle identifier required for clear")
                    return
                }
                UserDefaults.standard.removePersistentDomain(forName: bundleId)
                UserDefaults.standard.synchronize()
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

    private static func validateKeySegment(_ key: String) throws {
        if key.contains("..") || key.contains("/") || key.contains("\\") || key.contains("\u{0000}") {
            throw NVJsonWire.WireError.invalidArgs
        }
    }

    private static func setUserDefaultsValue(_ value: Any, forKey key: String) {
        switch value {
        case let b as Bool:
            UserDefaults.standard.set(b, forKey: key)
        case let i as Int:
            UserDefaults.standard.set(i, forKey: key)
        case let i as Int64:
            UserDefaults.standard.set(i, forKey: key)
        case let d as Double:
            UserDefaults.standard.set(d, forKey: key)
        case let f as Float:
            UserDefaults.standard.set(f, forKey: key)
        case let n as NSNumber:
            if CFGetTypeID(n) == CFBooleanGetTypeID() {
                UserDefaults.standard.set(n.boolValue, forKey: key)
            } else {
                UserDefaults.standard.set(n.doubleValue, forKey: key)
            }
        case let s as String:
            UserDefaults.standard.set(s, forKey: key)
        default:
            UserDefaults.standard.set(String(describing: value), forKey: key)
        }
    }

    private static func getTyped(key: String, type: String) -> Any {
        let ud = UserDefaults.standard
        switch type {
        case "boolean":
            return ud.bool(forKey: key)
        case "int":
            return ud.integer(forKey: key)
        case "long":
            if let n = ud.object(forKey: key) as? Int64 {
                return n
            }
            return Int64(ud.integer(forKey: key))
        case "float":
            return Double(ud.float(forKey: key))
        default:
            return ud.string(forKey: key) ?? ""
        }
    }
}
