// SPDX-License-Identifier: Apache-2.0

import Foundation

// MARK: - Bridge callbacks (desktop / Android parity)

public typealias MessageCallback = (String, String) -> Void

public typealias ReadyCallback = () -> Void

public typealias CloseCallback = () -> Void

/// Exact-method bridge handler; receives the ``NativeViewWindow`` that delivered the wire message (for UI + push events).
public typealias NVHandlerFn = (
    NativeViewWindow,
    String,
    @escaping ResolveCallback,
    @escaping RejectCallback
) -> Void

public typealias ResolveCallback = (String) -> Void

public typealias RejectCallback = (String, String) -> Void

/// Wire / handler error codes surfaced to JavaScript (align with `INVALID_ARGS` for malformed handler arguments).
public enum NVRejectCode {
    public static let invalidArgs = "INVALID_ARGS"
}

// MARK: - Runtime permissions (declarative; enforced by NVPermissionManager)

public enum NVPermission {
    case camera
    case location
    case microphone
    case photoLibrary
    case notifications
    case biometric
    case contacts
}
