// SPDX-License-Identifier: Apache-2.0

import Foundation

/// Window shell options for ``NativeViewApp/createWindow(config:)`` (parity with desktop bindings).
public struct NativeViewConfig: Sendable {
    public var title: String
    public var width: Int
    public var height: Int
    public var resizable: Bool
    public var devtools: Bool

    public init(
        title: String = "",
        width: Int = 390,
        height: Int = 844,
        resizable: Bool = true,
        devtools: Bool = false
    ) {
        self.title = title
        self.width = width
        self.height = height
        self.resizable = resizable
        self.devtools = devtools
    }
}
