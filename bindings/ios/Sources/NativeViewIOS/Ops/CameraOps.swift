// SPDX-License-Identifier: Apache-2.0

import Foundation
import ObjectiveC
import PhotosUI
import UIKit

/// Camera capture and photo library pick (`camera.takePicture`, `camera.pickImage`).
public enum CameraOps {
    public static let namespace = "camera"

    private static var coordKey: UInt8 = 0
    private static let busyLock = NSLock()
    private static var busy = false

    public static func register(app: NativeViewApp) {
        app.handle("camera.takePicture", permissions: [.camera]) { window, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    let (quality, front) = try pickArgs(argsJson)
                    guard let presenter = topPresenter(from: window) else {
                        reject("ERR_INTERNAL", "no view controller")
                        return
                    }
                    guard tryBeginBusy() else {
                        reject("ERR_IO", "camera busy")
                        return
                    }
                    guard UIImagePickerController.isSourceTypeAvailable(.camera) else {
                        endBusy()
                        reject("ERR_NOT_SUPPORTED", "no camera")
                        return
                    }
                    let picker = UIImagePickerController()
                    picker.sourceType = .camera
                    picker.cameraCaptureMode = .photo
                    if UIImagePickerController.isCameraDeviceAvailable(.front),
                       UIImagePickerController.isCameraDeviceAvailable(.rear)
                    {
                        picker.cameraDevice = front ? .front : .rear
                    }
                    let coord = UIImagePickerCoordinator(
                        quality: quality,
                        resolve: resolve,
                        reject: reject
                    )
                    objc_setAssociatedObject(
                        picker,
                        &coordKey,
                        coord,
                        .OBJC_ASSOCIATION_RETAIN_NONATOMIC
                    )
                    picker.delegate = coord
                    picker.modalPresentationStyle = .fullScreen
                    presenter.present(picker, animated: true)
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }

        app.handle("camera.pickImage", permissions: [.photoLibrary]) { window, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    let (quality, _) = try pickArgs(argsJson)
                    guard let presenter = topPresenter(from: window) else {
                        reject("ERR_INTERNAL", "no view controller")
                        return
                    }
                    guard tryBeginBusy() else {
                        reject("ERR_IO", "picker busy")
                        return
                    }
                    var config = PHPickerConfiguration(photoLibrary: .shared())
                    config.filter = .images
                    config.selectionLimit = 1
                    let picker = PHPickerViewController(configuration: config)
                    let coord = PHPickerCoordinator(
                        quality: quality,
                        resolve: resolve,
                        reject: reject
                    )
                    objc_setAssociatedObject(
                        picker,
                        &coordKey,
                        coord,
                        .OBJC_ASSOCIATION_RETAIN_NONATOMIC
                    )
                    picker.delegate = coord
                    picker.modalPresentationStyle = .formSheet
                    presenter.present(picker, animated: true)
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }
    }

    private static func pickArgs(_ argsJson: String) throws -> (quality: Int, front: Bool) {
        let a = try NVJsonWire.decodeObject(argsJson: argsJson)
        let q = NVJsonWire.int(a, key: "quality", default: 90)
        let front = NVJsonWire.bool(a, key: "front")
        return (max(1, min(100, q)), front)
    }

    private static func topPresenter(from window: NativeViewWindow) -> UIViewController? {
        var vc = window.viewController()
        while let p = vc.presentedViewController {
            vc = p
        }
        return vc
    }

    private static func tryBeginBusy() -> Bool {
        busyLock.lock()
        defer { busyLock.unlock() }
        if busy { return false }
        busy = true
        return true
    }

    fileprivate static func endBusy() {
        busyLock.lock()
        busy = false
        busyLock.unlock()
    }

    private static func resolveImagePayload(data: Data, format: String, resolve: @escaping ResolveCallback, reject: RejectCallback) {
        let b64 = data.base64EncodedString()
        let obj: [String: Any] = ["data": b64, "format": format]
        guard let out = NVJsonWire.jsonString(obj) else {
            reject("ERR_JSON", "encode failed")
            return
        }
        resolve(out)
    }

    private final class UIImagePickerCoordinator: NSObject, UIImagePickerControllerDelegate, UINavigationControllerDelegate {
        let quality: Int
        let resolve: ResolveCallback
        let reject: RejectCallback

        init(quality: Int, resolve: @escaping ResolveCallback, reject: @escaping RejectCallback) {
            self.quality = quality
            self.resolve = resolve
            self.reject = reject
        }

        func imagePickerControllerDidCancel(_ picker: UIImagePickerController) {
            picker.dismiss(animated: true) { [self] in
                objc_setAssociatedObject(picker, &coordKey, nil, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
                CameraOps.endBusy()
                self.reject("ERR_CANCELLED", "cancelled")
            }
        }

        func imagePickerController(_ picker: UIImagePickerController, didFinishPickingMediaWithInfo info: [UIImagePickerController.InfoKey: Any]) {
            picker.dismiss(animated: true) { [self] in
                objc_setAssociatedObject(picker, &coordKey, nil, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
                CameraOps.endBusy()
                guard let img = info[.originalImage] as? UIImage else {
                    self.reject("ERR_IO", "no image")
                    return
                }
                let q = CGFloat(self.quality) / 100.0
                guard let data = img.jpegData(compressionQuality: q) else {
                    self.reject("ERR_IO", "encode failed")
                    return
                }
                CameraOps.resolveImagePayload(data: data, format: "jpeg", resolve: self.resolve, reject: self.reject)
            }
        }
    }

    private final class PHPickerCoordinator: NSObject, PHPickerViewControllerDelegate {
        let quality: Int
        let resolve: ResolveCallback
        let reject: RejectCallback

        init(quality: Int, resolve: @escaping ResolveCallback, reject: @escaping RejectCallback) {
            self.quality = quality
            self.resolve = resolve
            self.reject = reject
        }

        func picker(_ picker: PHPickerViewController, didFinishPicking results: [PHPickerResult]) {
            picker.dismiss(animated: true) { [self] in
                objc_setAssociatedObject(picker, &coordKey, nil, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
                guard let first = results.first else {
                    CameraOps.endBusy()
                    self.reject("ERR_CANCELLED", "cancelled")
                    return
                }
                let provider = first.itemProvider
                if provider.canLoadObject(ofClass: UIImage.self) {
                    provider.loadObject(ofClass: UIImage.self) { [self] obj, err in
                        DispatchQueue.main.async { [self] in
                            CameraOps.endBusy()
                            if let err {
                                self.reject("ERR_IO", err.localizedDescription)
                                return
                            }
                            guard let img = obj as? UIImage else {
                                self.reject("ERR_IO", "no image")
                                return
                            }
                            let q = CGFloat(self.quality) / 100.0
                            guard let data = img.jpegData(compressionQuality: q) else {
                                self.reject("ERR_IO", "encode failed")
                                return
                            }
                            CameraOps.resolveImagePayload(
                                data: data,
                                format: "jpeg",
                                resolve: self.resolve,
                                reject: self.reject
                            )
                        }
                    }
                } else {
                    CameraOps.endBusy()
                    self.reject("ERR_NOT_SUPPORTED", "cannot load image")
                }
            }
        }
    }
}
