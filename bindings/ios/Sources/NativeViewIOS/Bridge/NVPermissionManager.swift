// SPDX-License-Identifier: Apache-2.0

import AVFoundation
import Contacts
import CoreLocation
import Foundation
import LocalAuthentication
import Photos
import UIKit
import UserNotifications

/// Tracks and requests iOS permissions required by ops (camera, location, notifications, etc.).
/// Subclass in tests (`@testable`) to simulate grant/deny flows without system dialogs.
public class NVPermissionManager: @unchecked Sendable {
    private let locationRequester = LocationPermissionRequester()

    public init() {}

    /// Runs `then` on a background queue after all listed permissions are granted (or immediately when the list is empty).
    /// Runs `denied` on the main queue when any required permission is denied or restricted.
    open func check(
        permissions: [NVPermission],
        then: @escaping () -> Void,
        denied: @escaping () -> Void
    ) {
        let unique = Self.dedupe(permissions)
        if unique.isEmpty {
            DispatchQueue.global(qos: .userInitiated).async(execute: then)
            return
        }
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.runChain(unique, index: 0) { ok in
                if ok {
                    DispatchQueue.global(qos: .userInitiated).async(execute: then)
                } else {
                    denied()
                }
            }
        }
    }

    private func runChain(_ permissions: [NVPermission], index: Int, completion: @escaping (Bool) -> Void) {
        if index >= permissions.count {
            completion(true)
            return
        }
        let p = permissions[index]
        request(for: p) { [weak self] granted in
            guard let self else { return }
            if !granted {
                completion(false)
                return
            }
            self.runChain(permissions, index: index + 1, completion: completion)
        }
    }

    private func request(for permission: NVPermission, completion: @escaping (Bool) -> Void) {
        switch permission {
        case .camera:
            requestCamera(completion: completion)
        case .location:
            locationRequester.requestWhenInUse(completion: completion)
        case .photoLibrary:
            requestPhotoLibrary(completion: completion)
        case .microphone:
            requestMicrophone(completion: completion)
        case .notifications:
            requestNotifications(completion: completion)
        case .biometric:
            requestBiometric(completion: completion)
        case .contacts:
            requestContacts(completion: completion)
        }
    }

    private func requestCamera(completion: @escaping (Bool) -> Void) {
        switch AVCaptureDevice.authorizationStatus(for: .video) {
        case .authorized:
            completion(true)
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .video) { granted in
                DispatchQueue.main.async {
                    completion(granted)
                }
            }
        default:
            completion(false)
        }
    }

    private func requestMicrophone(completion: @escaping (Bool) -> Void) {
        let session = AVAudioSession.sharedInstance()
        switch session.recordPermission {
        case .granted:
            completion(true)
        case .denied:
            completion(false)
        case .undetermined:
            session.requestRecordPermission { granted in
                DispatchQueue.main.async {
                    completion(granted)
                }
            }
        @unknown default:
            completion(false)
        }
    }

    private func requestPhotoLibrary(completion: @escaping (Bool) -> Void) {
        let status: PHAuthorizationStatus
        if #available(iOS 14, *) {
            status = PHPhotoLibrary.authorizationStatus(for: .readWrite)
        } else {
            status = PHPhotoLibrary.authorizationStatus()
        }
        switch status {
        case .authorized, .limited:
            completion(true)
        case .notDetermined:
            if #available(iOS 14, *) {
                PHPhotoLibrary.requestAuthorization(for: .readWrite) { newStatus in
                    DispatchQueue.main.async {
                        switch newStatus {
                        case .authorized, .limited:
                            completion(true)
                        default:
                            completion(false)
                        }
                    }
                }
            } else {
                PHPhotoLibrary.requestAuthorization { newStatus in
                    DispatchQueue.main.async {
                        completion(newStatus == .authorized)
                    }
                }
            }
        default:
            completion(false)
        }
    }

    private func requestNotifications(completion: @escaping (Bool) -> Void) {
        let center = UNUserNotificationCenter.current()
        center.getNotificationSettings { settings in
            DispatchQueue.main.async {
                switch settings.authorizationStatus {
                case .authorized, .provisional, .ephemeral:
                    completion(true)
                case .denied:
                    completion(false)
                case .notDetermined:
                    center.requestAuthorization(options: [.alert, .badge, .sound]) { granted, _ in
                        DispatchQueue.main.async {
                            completion(granted)
                        }
                    }
                @unknown default:
                    completion(false)
                }
            }
        }
    }

    /// Ensures biometric hardware is available and enrolled (does **not** show the system auth UI; that is reserved for ``BiometricOps``).
    private func requestBiometric(completion: @escaping (Bool) -> Void) {
        let ctx = LAContext()
        var error: NSError?
        let ok = ctx.canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics, error: &error)
        DispatchQueue.main.async {
            completion(ok)
        }
    }

    private func requestContacts(completion: @escaping (Bool) -> Void) {
        let status = CNContactStore.authorizationStatus(for: .contacts)
        switch status {
        case .authorized:
            completion(true)
        case .notDetermined:
            CNContactStore().requestAccess(for: .contacts) { granted, _ in
                DispatchQueue.main.async {
                    completion(granted)
                }
            }
        default:
            completion(false)
        }
    }

    private static func dedupe(_ permissions: [NVPermission]) -> [NVPermission] {
        var seen = Set<String>()
        var out: [NVPermission] = []
        for p in permissions {
            let key = String(describing: p)
            if seen.insert(key).inserted {
                out.append(p)
            }
        }
        return out
    }
}

// MARK: - Location (one-shot CLLocationManager)

private final class LocationPermissionRequester: NSObject, CLLocationManagerDelegate {
    private let manager = CLLocationManager()
    private var completion: ((Bool) -> Void)?

    override init() {
        super.init()
        manager.delegate = self
    }

    func requestWhenInUse(completion: @escaping (Bool) -> Void) {
        let status: CLAuthorizationStatus
        if #available(iOS 14.0, *) {
            status = manager.authorizationStatus
        } else {
            status = CLLocationManager.authorizationStatus()
        }
        switch status {
        case .authorizedAlways, .authorizedWhenInUse:
            completion(true)
        case .denied, .restricted:
            completion(false)
        case .notDetermined:
            self.completion = completion
            manager.requestWhenInUseAuthorization()
        @unknown default:
            completion(false)
        }
    }

    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        guard let completion else { return }
        let status: CLAuthorizationStatus
        if #available(iOS 14.0, *) {
            status = manager.authorizationStatus
        } else {
            status = CLLocationManager.authorizationStatus()
        }
        switch status {
        case .authorizedAlways, .authorizedWhenInUse:
            self.completion = nil
            completion(true)
        case .denied, .restricted:
            self.completion = nil
            completion(false)
        case .notDetermined:
            break
        @unknown default:
            break
        }
    }
}
