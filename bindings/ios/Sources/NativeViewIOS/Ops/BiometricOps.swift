// SPDX-License-Identifier: Apache-2.0

import Foundation
import LocalAuthentication
import UIKit

/// Face ID / Touch ID bridge (`biometric.*`).
public enum BiometricOps {
    public static let namespace = "biometric"

    public static func register(app: NativeViewApp) {
        app.handle("biometric.isAvailable", permissions: []) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                    let ctx = LAContext()
                    var error: NSError?
                    let can = ctx.canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics, error: &error)
                    let type: String
                    if can {
                        switch ctx.biometryType {
                        case .faceID, .opticID:
                            type = "faceID"
                        case .touchID:
                            type = "touchID"
                        case .none:
                            type = "none"
                        @unknown default:
                            type = "none"
                        }
                    } else {
                        type = "none"
                    }
                    let obj: [String: Any] = ["available": can, "type": type]
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

        app.handle("biometric.authenticate", permissions: [.biometric]) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    let a = try NVJsonWire.decodeObject(argsJson: argsJson)
                    let reason = NVJsonWire.string(a, key: "reason", default: "Authenticate")
                    let ctx = LAContext()
                    var error: NSError?
                    guard ctx.canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics, error: &error) else {
                        reject("PERMISSION_DENIED", "Biometry not available")
                        return
                    }
                    ctx.evaluatePolicy(.deviceOwnerAuthenticationWithBiometrics, localizedReason: reason) { success, _ in
                        if success {
                            guard let out = NVJsonWire.jsonString([:]) else {
                                reject("ERR_JSON", "encode failed")
                                return
                            }
                            resolve(out)
                        } else {
                            reject("PERMISSION_DENIED", "Biometric authentication failed")
                        }
                    }
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }
    }
}
