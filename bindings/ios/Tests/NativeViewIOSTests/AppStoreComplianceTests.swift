// SPDX-License-Identifier: Apache-2.0

import XCTest

/// Static guardrails for App Store–sensitive patterns in `Sources/NativeViewIOS` and the Example app.
final class AppStoreComplianceTests: XCTestCase {
    private var packageRoot: URL {
        URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
    }

    private func swiftFiles(under dir: URL) -> [URL] {
        var out: [URL] = []
        guard let e = FileManager.default.enumerator(
            at: dir,
            includingPropertiesForKeys: [.isRegularFileKey],
            options: [.skipsHiddenFiles]
        ) else {
            return out
        }
        for case let u as URL in e {
            if u.pathExtension.lowercased() == "swift" {
                out.append(u)
            }
        }
        return out.sorted { $0.path < $1.path }
    }

    private func readText(_ url: URL) -> String {
        (try? String(contentsOf: url, encoding: .utf8)) ?? ""
    }

    func testSourcesContainNoProcessExit() {
        let root = packageRoot.appendingPathComponent("Sources/NativeViewIOS")
        XCTAssertTrue(FileManager.default.fileExists(atPath: root.path), "Expected \(root.path)")
        for u in swiftFiles(under: root) {
            let s = readText(u)
            XCTAssertFalse(
                s.range(of: #"\bexit\s*\("#, options: .regularExpression) != nil,
                "Avoid Darwin/C exit(); file: \(u.lastPathComponent)"
            )
        }
    }

    func testSourcesDoNotReferenceUIWebView() {
        let root = packageRoot.appendingPathComponent("Sources/NativeViewIOS")
        for u in swiftFiles(under: root) {
            let s = readText(u)
            XCTAssertFalse(
                s.contains("UIWebView"),
                "UIWebView is banned; use WKWebView — \(u.lastPathComponent)"
            )
        }
    }

    func testExampleProjectDeclaresPrivacyUsageDescriptions() {
        let pbx = packageRoot
            .appendingPathComponent("Example/ExampleApp.xcodeproj/project.pbxproj")
        XCTAssertTrue(FileManager.default.fileExists(atPath: pbx.path), "Missing \(pbx.path)")
        let text = readText(pbx)
        let requiredKeys = [
            "INFOPLIST_KEY_NSCameraUsageDescription",
            "INFOPLIST_KEY_NSPhotoLibraryUsageDescription",
            "INFOPLIST_KEY_NSLocationWhenInUseUsageDescription",
            "INFOPLIST_KEY_NSFaceIDUsageDescription",
            "INFOPLIST_KEY_NSContactsUsageDescription",
            "INFOPLIST_KEY_NSMicrophoneUsageDescription",
        ]
        for k in requiredKeys {
            XCTAssertTrue(text.contains(k), "Example target must declare \(k) for default bridge permissions.")
        }
    }
}
