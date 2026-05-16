// SPDX-License-Identifier: Apache-2.0

import Foundation
import UIKit

/// `UIPasteboard.general` text bridge (`clipboard.*`).
public enum ClipboardOps {
    public static let namespace = "clipboard"

    public static func register(app: NativeViewApp) {
        app.handle("clipboard.readText", permissions: []) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                    let text = UIPasteboard.general.string ?? ""
                    let obj: [String: Any] = ["text": text]
                    guard let out = NVJsonWire.jsonString(obj) else {
                        reject("ERR_JSON", "encode failed")
                        return
                    }
                    resolve(out)
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }

        app.handle("clipboard.writeText", permissions: []) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    let a = try NVJsonWire.decodeObject(argsJson: argsJson)
                    let text = NVJsonWire.string(a, key: "text")
                    UIPasteboard.general.string = text
                    guard let out = NVJsonWire.jsonString([:]) else {
                        reject("ERR_JSON", "encode failed")
                        return
                    }
                    resolve(out)
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }

        app.handle("clipboard.clear", permissions: []) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                    UIPasteboard.general.string = ""
                    guard let out = NVJsonWire.jsonString([:]) else {
                        reject("ERR_JSON", "encode failed")
                        return
                    }
                    resolve(out)
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }
    }
}
