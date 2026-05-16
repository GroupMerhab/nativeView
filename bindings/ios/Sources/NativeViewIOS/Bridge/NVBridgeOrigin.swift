// SPDX-License-Identifier: Apache-2.0

import Foundation

/// Normalizes document origins for ``NativeViewApp/allowOrigin(_:)`` (parity with Android `BridgeOrigin`).
enum NVBridgeOrigin {
    static func normalizeAllowedOrigin(_ origin: String) -> String? {
        let trimmed = origin.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty {
            return nil
        }
        guard let url = URL(string: trimmed), let scheme = url.scheme?.lowercased() else {
            return nil
        }
        if scheme == "file" {
            return "file://"
        }
        guard let host = url.host?.lowercased(), !host.isEmpty else {
            return nil
        }
        let port = effectivePort(scheme: scheme, port: url.port)
        if let port, port > 0 {
            return "\(scheme)://\(host):\(port)"
        }
        return "\(scheme)://\(host)"
    }

    static func normalizePageURL(_ pageURL: URL?) -> String? {
        guard let pageURL, let scheme = pageURL.scheme?.lowercased() else {
            return nil
        }
        if scheme == "file" {
            return "file://"
        }
        guard let host = pageURL.host?.lowercased(), !host.isEmpty else {
            return nil
        }
        let port = effectivePort(scheme: scheme, port: pageURL.port)
        if let port, port > 0 {
            return "\(scheme)://\(host):\(port)"
        }
        return "\(scheme)://\(host)"
    }

    /// True when the document URL’s origin is on the allow list, or when `file://` is allowed and the page is a `file:` URL.
    static func isAllowed(pageURL: URL?, allowed: Set<String>) -> Bool {
        guard let pageURL else {
            return false
        }
        if let n = normalizePageURL(pageURL), allowed.contains(n) {
            return true
        }
        let scheme = (pageURL.scheme ?? "").lowercased()
        if scheme == "file", allowed.contains("file://") {
            return true
        }
        return false
    }

    private static func effectivePort(scheme: String, port: Int?) -> Int? {
        guard let port else {
            return nil
        }
        let s = scheme.lowercased()
        if s == "https", port == 443 { return nil }
        if s == "http", port == 80 { return nil }
        if s == "wss", port == 443 { return nil }
        if s == "ws", port == 80 { return nil }
        return port
    }
}
