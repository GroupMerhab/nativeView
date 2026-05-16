// SPDX-License-Identifier: Apache-2.0
//
// Thin Swift layer over the nativeview C API (NativeView.xcframework).
// Prefer these helpers over raw `UnsafePointer` / C string handling in app code.

import Foundation
import NativeView

/// Swift-friendly wrappers for selected C entry points from `nv.h` / `nv_util.h`.
public enum NVCBridge {
    /// Copies `nv_version_string()` into a Swift `String` (never force-unwraps C pointers).
    public static func versionString() -> String {
        guard let c = nv_version_string() else {
            return ""
        }
        return String(cString: c)
    }

    /// Reads `nv_get_version_info` into Swift value types.
    public static func versionInfo() -> (major: UInt16, minor: UInt16, patch: UInt16, buildSha: String?) {
        var info = NvVersionInfo()
        nv_get_version_info(&info)
        let sha: String?
        if let p = info.build_sha {
            sha = String(cString: p)
        } else {
            sha = nil
        }
        return (info.major, info.minor, info.patch, sha)
    }

    public static func benchNow() -> UInt64 {
        nv_bench_now()
    }

    /// Converts an optional NUL-terminated UTF-8 C string to Swift (empty when `NULL`).
    public static func string(fromCString ptr: UnsafePointer<CChar>?) -> String {
        guard let ptr else {
            return ""
        }
        return String(cString: ptr)
    }

    /// Pass a Swift object through a C `void*` userdata pointer (pair with `releaseUserdata`).
    public static func retainUserdata<T: AnyObject>(_ object: T) -> UnsafeMutableRawPointer {
        Unmanaged.passRetained(object).toOpaque()
    }

    /// Releases the reference created by `retainUserdata` once the C callback will not fire again.
    public static func releaseUserdata<T: AnyObject>(_ ptr: UnsafeMutableRawPointer?, as _: T.Type) {
        guard let ptr else { return }
        Unmanaged<T>.fromOpaque(ptr).release()
    }
}
