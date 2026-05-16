// SPDX-License-Identifier: Apache-2.0

import Foundation

/// Parses compact wire `d` JSON objects for ``NVHandlerFn`` handlers (Android `BridgeArgs` parity).
enum NVJsonWire {
    enum WireError: Error, Equatable {
        case invalidArgs
        case invalidJson
    }

    static func decodeObject(argsJson: String) throws -> [String: Any] {
        if argsJson == "null" { return [:] }
        guard let data = argsJson.data(using: .utf8) else {
            throw WireError.invalidArgs
        }
        let top: Any
        do {
            top = try JSONSerialization.jsonObject(with: data, options: [])
        } catch {
            throw WireError.invalidJson
        }
        guard let obj = top as? [String: Any] else {
            throw WireError.invalidArgs
        }
        return obj
    }

    /// Decodes `d` JSON as a `Decodable` value (fails with ``WireError/invalidArgs`` when shape does not match).
    static func decode<T: Decodable>(_ type: T.Type, from argsJson: String, decoder: JSONDecoder = JSONDecoder()) throws -> T {
        let obj = try decodeObject(argsJson: argsJson)
        guard let nested = try? JSONSerialization.data(withJSONObject: obj) else {
            throw WireError.invalidArgs
        }
        do {
            return try decoder.decode(T.self, from: nested)
        } catch {
            throw WireError.invalidArgs
        }
    }

    static func jsonString(_ obj: [String: Any]) -> String? {
        guard JSONSerialization.isValidJSONObject(obj),
              let data = try? JSONSerialization.data(withJSONObject: obj, options: [])
        else {
            return nil
        }
        return String(data: data, encoding: .utf8)
    }

    static func string(_ args: [String: Any], key: String, default def: String = "") -> String {
        if let s = args[key] as? String { return s }
        if let n = args[key] as? NSNumber { return n.stringValue }
        return def
    }

    static func bool(_ args: [String: Any], key: String, default def: Bool = false) -> Bool {
        if let b = args[key] as? Bool { return b }
        if let n = args[key] as? NSNumber { return n.boolValue }
        return def
    }

    static func int(_ args: [String: Any], key: String, default def: Int = 0) -> Int {
        if let i = args[key] as? Int { return i }
        if let n = args[key] as? NSNumber { return n.intValue }
        return def
    }

    static func double(_ args: [String: Any], key: String, default def: Double = 0) -> Double {
        if let d = args[key] as? Double { return d }
        if let n = args[key] as? NSNumber { return n.doubleValue }
        return def
    }
}
