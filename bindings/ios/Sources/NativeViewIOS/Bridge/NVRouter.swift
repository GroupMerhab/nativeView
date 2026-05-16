// SPDX-License-Identifier: Apache-2.0

import Foundation

/// Dispatches bridge method keys (e.g. `device.*`, `storage.*`) to registered ``NVHandler``s,
/// and exact method keys (e.g. `app.custom`) to ``NVHandlerFn`` routes from ``NativeViewApp/handle(_:permissions:handler:)``.
public final class NVRouter: @unchecked Sendable {
    public struct MethodRoute {
        public let permissions: [NVPermission]
        public let handler: NVHandlerFn
    }

    private var handlers: [String: NVHandler] = [:]
    private var methodRoutes: [String: MethodRoute] = [:]
    private let lock = NSLock()

    public init() {}

    public func register(prefix: String, handler: NVHandler) {
        lock.lock()
        defer { lock.unlock() }
        handlers[prefix] = handler
    }

    /// Registers an exact bridge method (e.g. `my.namespace.action`).
    public func register(method: String, permissions: [NVPermission] = [], handler: @escaping NVHandlerFn) {
        lock.lock()
        defer { lock.unlock() }
        methodRoutes[method] = MethodRoute(permissions: permissions, handler: handler)
    }

    public func methodRoute(forExactKey key: String) -> MethodRoute? {
        lock.lock()
        defer { lock.unlock() }
        return methodRoutes[key]
    }

    public func handler(forMethodKey key: String) -> NVHandler? {
        lock.lock()
        defer { lock.unlock() }
        for (prefix, handler) in handlers where key.hasPrefix(prefix) {
            return handler
        }
        return nil
    }

    /// Dispatches an exact-registered method (``register(method:permissions:handler:)``).
    /// - Returns: `true` if a handler was invoked; `false` if no exact route exists (caller may try prefix ``NVHandler`` routes).
    @discardableResult
    public func route(
        method: String,
        window: NativeViewWindow,
        args: String,
        resolve: @escaping ResolveCallback,
        reject: @escaping RejectCallback
    ) -> Bool {
        lock.lock()
        let route = methodRoutes[method]
        lock.unlock()
        guard let route else { return false }
        route.handler(window, args, resolve, reject)
        return true
    }

    func clearMethodRoutes() {
        lock.lock()
        defer { lock.unlock() }
        methodRoutes.removeAll()
    }

    func clearPrefixHandlers() {
        lock.lock()
        defer { lock.unlock() }
        handlers.removeAll()
    }
}
