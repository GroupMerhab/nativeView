// SPDX-License-Identifier: Apache-2.0

import Foundation

/// Escapes arbitrary Swift strings as JavaScript string literals for `evaluateJavaScript` (JSON `string` production).
enum NVJavaScriptStringEncoding {
    /// Encodes `s` as a JSON string literal (safe to embed in JavaScript source). Uses an array wrapper because
    /// `JSONSerialization` does not accept a bare `String` as a top-level object on all Apple platforms.
    static func jsStringLiteral(_ s: String) -> String {
        guard let d = try? JSONSerialization.data(withJSONObject: [s], options: []),
              let wrapped = String(data: d, encoding: .utf8),
              wrapped.hasPrefix("["),
              wrapped.hasSuffix("]")
        else {
            return "\"\""
        }
        return String(wrapped.dropFirst().dropLast())
    }

    /// Same as ``jsStringLiteralFromArraySlice`` but returns `nil` when encoding fails (for JSON fragments embedded in wire replies).
    static func jsStringLiteralFromArraySliceOrNil(_ s: String) -> String? {
        guard let d = try? JSONSerialization.data(withJSONObject: [s], options: []),
              let wrapped = String(data: d, encoding: .utf8),
              wrapped.hasPrefix("["),
              wrapped.hasSuffix("]")
        else {
            return nil
        }
        return String(wrapped.dropFirst().dropLast())
    }
}
