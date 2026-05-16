// SPDX-License-Identifier: Apache-2.0

import Foundation
import UIKit

/// App metadata (`app.*`). `app.exit` is unavailable on iOS (App Store policy).
public enum AppOps {
    public static let namespace = "app"

    public static func register(app: NativeViewApp) {
        app.handle("app.getVersion", permissions: []) { _, argsJson, resolve, reject in
            do {
                _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                let bundle = Bundle.main
                let version = bundle.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? ""
                let build = bundle.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? ""
                let obj: [String: Any] = ["version": version, "build": build]
                guard let out = NVJsonWire.jsonString(obj) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                reject(NVRejectCode.invalidArgs, "malformed arguments")
            }
        }

        app.handle("app.getPlatform", permissions: []) { _, argsJson, resolve, reject in
            do {
                _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                let obj: [String: Any] = ["platform": "ios"]
                guard let out = NVJsonWire.jsonString(obj) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                reject(NVRejectCode.invalidArgs, "malformed arguments")
            }
        }

        app.handle("app.getInfo", permissions: []) { _, argsJson, resolve, reject in
            do {
                _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                let bundle = Bundle.main
                let bundleId = bundle.bundleIdentifier ?? ""
                var isDebug = false
                #if DEBUG
                    isDebug = true
                #endif
                let obj: [String: Any] = [
                    "bundleId": bundleId,
                    "buildType": isDebug ? "debug" : "release",
                ]
                guard let out = NVJsonWire.jsonString(obj) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                reject(NVRejectCode.invalidArgs, "malformed arguments")
            }
        }

        app.handle("app.exit", permissions: []) { _, argsJson, _, reject in
            do {
                _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                reject("NOT_AVAILABLE", "iOS does not allow programmatic app exit")
            } catch {
                reject(NVRejectCode.invalidArgs, "malformed arguments")
            }
        }
    }
}
