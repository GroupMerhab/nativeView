// SPDX-License-Identifier: Apache-2.0

import Foundation

/// Parses nativeview compact wire JSON (`e`, `d`, `s`) for the iOS Swift bridge.
enum NVWireHelpers {
    static func parseMethod(_ wireJson: String) -> String? {
        guard let data = wireJson.data(using: .utf8),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let e = obj["e"] as? String,
              !e.isEmpty
        else {
            return nil
        }
        return e
    }

    static func extractSeq(_ wireJson: String) -> Int64 {
        guard let data = wireJson.data(using: .utf8),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        else {
            return 0
        }
        if let n = obj["s"] as? Int64 { return n }
        if let n = obj["s"] as? Int { return Int64(n) }
        if let n = obj["s"] as? NSNumber { return n.int64Value }
        return 0
    }

    /// Serializes the `d` field for ``NVHandlerFn`` (matches Android `WireJson.extractArgsJson`).
    static func extractArgsJson(_ wireJson: String) -> String? {
        guard let data = wireJson.data(using: .utf8),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        else {
            return nil
        }
        guard obj.keys.contains("d") else {
            return "null"
        }
        if obj["d"] is NSNull || obj["d"] == nil {
            return "null"
        }
        guard let d = obj["d"] else {
            return "null"
        }
        if let s = d as? String {
            return jsonQuote(s)
        }
        if let b = d as? Bool {
            return b ? "true" : "false"
        }
        if let n = d as? NSNumber {
            if CFGetTypeID(n) == CFBooleanGetTypeID() {
                return n.boolValue ? "true" : "false"
            }
            return n.stringValue
        }
        if JSONSerialization.isValidJSONObject(d),
           let nested = try? JSONSerialization.data(withJSONObject: d, options: []),
           let str = String(data: nested, encoding: .utf8)
        {
            return str
        }
        return jsonQuote(String(describing: d))
    }

    private static func jsonQuote(_ s: String) -> String {
        guard let data = try? JSONSerialization.data(withJSONObject: [s], options: []),
              let wrapped = String(data: data, encoding: .utf8),
              wrapped.hasPrefix("["),
              wrapped.hasSuffix("]")
        else {
            return "\"\""
        }
        return String(wrapped.dropFirst().dropLast())
    }
}
