// SPDX-License-Identifier: Apache-2.0
//
// iOS only — UIKit shell helpers (status bar, orientation, idle timer, navigation gestures,
// keyboard, safe-area pinning for the embedded WKWebView). Intentionally **not** part of the
// portable C API (`nv.h`), JNI, or the cross-platform `NativeView` JavaScript surface.

import UIKit

/// Shared helpers and metadata for the NativeViewIOS module.
public enum NativeViewIOSSupport {
    public static let moduleName = "NativeViewIOS"
}

extension Notification.Name {
    /// Posted on the main queue when ``NativeViewIOS/setSafeAreaEnabled(_:)`` changes.
    static let nativeViewIOSSafeAreaPreferenceDidChange = Notification.Name("NativeViewIOS.safeAreaPreferenceDidChange")
}

/// iOS only — UIKit integration helpers for apps embedding ``NativeViewWindow`` / ``NativeViewViewController``.
///
/// These entry points exist only in the **NativeViewIOS** Swift package. They are **not** mirrored in
/// `include/nv.h`, desktop/mobile JNI, or `nativeview.js` / the portable `NativeView` JS API.
public class NativeViewIOS {
    private static let lock = NSLock()

    private static var statusBarStyle: UIStatusBarStyle = .default
    private static var statusBarHidden: Bool = false
    private static var supportedOrientations: UIInterfaceOrientationMask = .all
    private static var swipeBackForcedOff: Bool = false
    private static var safeAreaInsetsEnabled: Bool = true

    // MARK: - Status bar (iOS only)

    /// iOS only — Sets the preferred status bar style for view-controller–based status bar appearance.
    /// Call from the main thread; updates are dispatched to the main queue if needed.
    public static func setStatusBarStyle(_ style: UIStatusBarStyle) {
        syncOnMain {
            lock.lock()
            statusBarStyle = style
            lock.unlock()
            notifyStatusBarAppearanceUpdate()
        }
    }

    /// iOS only — Sets whether the status bar should be hidden for view-controller–based appearance.
    public static func setStatusBarHidden(_ hidden: Bool) {
        syncOnMain {
            lock.lock()
            statusBarHidden = hidden
            lock.unlock()
            notifyStatusBarAppearanceUpdate()
        }
    }

    // MARK: - Orientation (iOS only)

    /// iOS only — Updates the mask returned by ``NativeViewViewController/supportedInterfaceOrientations``.
    /// Embed hosts may still need to align their container view controllers with this mask.
    public static func setSupportedOrientations(_ orientations: UIInterfaceOrientationMask) {
        syncOnMain {
            lock.lock()
            supportedOrientations = orientations
            lock.unlock()
            requestSupportedOrientationsRefresh()
        }
    }

    // MARK: - Screen (iOS only)

    /// iOS only — Controls `UIApplication.shared.isIdleTimerDisabled` (keep screen on).
    public static func setKeepScreenOn(_ enabled: Bool) {
        syncOnMain {
            UIApplication.shared.isIdleTimerDisabled = enabled
        }
    }

    // MARK: - Navigation (iOS only)

    /// iOS only — Forces the interactive pop gesture on the nearest enclosing ``UINavigationController`` off,
    /// in addition to the default behavior that disables pop when the ``WKWebView`` can go back.
    public static func disableSwipeBackGesture() {
        syncOnMain {
            lock.lock()
            swipeBackForcedOff = true
            lock.unlock()
            refreshAllPopGestures()
        }
    }

    /// iOS only — Clears the forced-off state from ``disableSwipeBackGesture()`` and reapplies the default rule.
    public static func enableSwipeBackGesture() {
        syncOnMain {
            lock.lock()
            swipeBackForcedOff = false
            lock.unlock()
            refreshAllPopGestures()
        }
    }

    // MARK: - Keyboard (iOS only)

    /// iOS only — Resigns first responder for the key window (dismisses keyboard / input accessories).
    public static func hideKeyboard() {
        syncOnMain {
            UIApplication.shared.sendAction(#selector(UIResponder.resignFirstResponder), to: nil, from: nil, for: nil)
        }
    }

    // MARK: - Safe area (iOS only)

    /// iOS only — When `true` (default), the ``NativeViewWebView`` is pinned to ``UIView/safeAreaLayoutGuide``.
    /// When `false`, edges attach to the host view so content may draw under bars and the home indicator.
    public static func setSafeAreaEnabled(_ enabled: Bool) {
        syncOnMain {
            lock.lock()
            safeAreaInsetsEnabled = enabled
            lock.unlock()
            NotificationCenter.default.post(
                name: .nativeViewIOSSafeAreaPreferenceDidChange,
                object: nil,
                userInfo: ["enabled": enabled]
            )
        }
    }

    // MARK: - Internal hooks (NativeViewIOS module)

    internal static func preferredStatusBarStyleForHosting() -> UIStatusBarStyle {
        lock.lock()
        let s = statusBarStyle
        lock.unlock()
        return s
    }

    internal static func prefersStatusBarHiddenForHosting() -> Bool {
        lock.lock()
        let h = statusBarHidden
        lock.unlock()
        return h
    }

    internal static func supportedInterfaceOrientationsForHosting() -> UIInterfaceOrientationMask {
        lock.lock()
        let m = supportedOrientations
        lock.unlock()
        return m
    }

    internal static func isSwipeBackGestureForcedOff() -> Bool {
        lock.lock()
        let v = swipeBackForcedOff
        lock.unlock()
        return v
    }

    internal static func isSafeAreaLayoutEnabled() -> Bool {
        lock.lock()
        let v = safeAreaInsetsEnabled
        lock.unlock()
        return v
    }

    private static func syncOnMain(_ work: @escaping () -> Void) {
        if Thread.isMainThread {
            work()
        } else {
            DispatchQueue.main.async(execute: work)
        }
    }

    private static func notifyStatusBarAppearanceUpdate() {
        for scene in UIApplication.shared.connectedScenes {
            guard let windowScene = scene as? UIWindowScene else { continue }
            for window in windowScene.windows {
                window.rootViewController?.nv_setNeedsStatusBarAppearanceUpdateRecursive()
            }
        }
    }

    private static func requestSupportedOrientationsRefresh() {
        if #available(iOS 16.0, *) {
            for scene in UIApplication.shared.connectedScenes {
                guard let windowScene = scene as? UIWindowScene else { continue }
                for window in windowScene.windows {
                    window.rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()
                }
            }
        }
        UIViewController.attemptRotationToDeviceOrientation()
    }

    fileprivate static func refreshAllPopGestures() {
        for scene in UIApplication.shared.connectedScenes {
            guard let windowScene = scene as? UIWindowScene else { continue }
            for window in windowScene.windows {
                guard let root = window.rootViewController else { continue }
                var stack: [UIViewController] = [root]
                while let vc = stack.popLast() {
                    if let nv = vc as? NativeViewViewController {
                        nv.webView.refreshPopGestureEnabled()
                    }
                    stack.append(contentsOf: vc.children)
                    if let presented = vc.presentedViewController {
                        stack.append(presented)
                    }
                }
            }
        }
    }
}

private extension UIViewController {
    func nv_setNeedsStatusBarAppearanceUpdateRecursive() {
        setNeedsStatusBarAppearanceUpdate()
        for child in children {
            child.nv_setNeedsStatusBarAppearanceUpdateRecursive()
        }
        presentedViewController?.nv_setNeedsStatusBarAppearanceUpdateRecursive()
    }
}
