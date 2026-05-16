// SPDX-License-Identifier: Apache-2.0

import Foundation

/// JavaScript snippets for `NativeView._resolve` / `NativeView._reject` (parity with Android `BridgeClientScript`).
enum NVWebBridgeClient {
    static func buildResolveInvoke(seq: Int64, json: String) -> String? {
        if seq <= 0 {
            return nil
        }
        let secondArg: String
        let trimmed = json.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty {
            secondArg = "null"
        } else {
            secondArg = "JSON.parse(\(jsonQuote(trimmed)))"
        }
        return "(function(){try{if(typeof NativeView!=='undefined'&&NativeView._resolve){NativeView._resolve(\(String(seq)),\(secondArg));}}catch(e){}})()"
    }

    static func buildRejectInvoke(seq: Int64, code: String, message: String) -> String? {
        if seq <= 0 {
            return nil
        }
        let c = jsonQuote(code.isEmpty ? "ERR_UNKNOWN" : code)
        let m = jsonQuote(message)
        return "(function(){try{if(typeof NativeView!=='undefined'&&NativeView._reject){NativeView._reject(\(String(seq)),{code:\(c),msg:\(m)});}}catch(e){}})()"
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
