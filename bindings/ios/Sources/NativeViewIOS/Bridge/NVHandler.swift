// SPDX-License-Identifier: Apache-2.0

import Foundation

/// Handles a namespace of bridge methods (prefix registration on ``NVRouter``).
/// For single-method JavaScript RPC handlers, use ``NVHandlerFn`` with ``NativeViewApp/handle(_:permissions:handler:)``.
public protocol NVHandler: AnyObject {
    func handle(methodKey: String, arguments: [String: Any]) async throws -> Any?
}
