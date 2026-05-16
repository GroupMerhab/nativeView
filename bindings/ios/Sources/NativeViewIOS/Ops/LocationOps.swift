// SPDX-License-Identifier: Apache-2.0

import CoreLocation
import Foundation
import UIKit

/// CoreLocation bridge (`location.get`, `location.watch`, `location.unwatch`).
public enum LocationOps {
    public static let namespace = "location"

    private static let lock = NSLock()
    private static var nextWatchId = 1
    private static var watchesByApp: [ObjectIdentifier: [Int: LocationWatch]] = [:]

    public static func register(app: NativeViewApp) {
        let appId = ObjectIdentifier(app)

        app.handle("location.get", permissions: [.location]) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                    let shot = OneShotGetter()
                    shot.request { result in
                        switch result {
                        case let .success(loc):
                            let obj: [String: Any] = [
                                "lat": loc.coordinate.latitude,
                                "lng": loc.coordinate.longitude,
                                "accuracy": loc.horizontalAccuracy,
                            ]
                            guard let out = NVJsonWire.jsonString(obj) else {
                                reject("ERR_JSON", "encode failed")
                                return
                            }
                            resolve(out)
                        case let .failure(err):
                            reject("ERR_IO", err.localizedDescription)
                        }
                    }
                } catch {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                }
            }
        }

        app.handle("location.watch", permissions: [.location]) { window, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    _ = try NVJsonWire.decodeObject(argsJson: argsJson)
                    let watchId = Self.allocWatchId()
                    let watch = LocationWatch(window: window)
                    lock.lock()
                    watchesByApp[appId, default: [:]][watchId] = watch
                    lock.unlock()
                    watch.start()
                    let obj: [String: Any] = ["watchId": watchId]
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

        app.handle("location.unwatch", permissions: []) { _, argsJson, resolve, reject in
            DispatchQueue.main.async {
                do {
                    let a = try NVJsonWire.decodeObject(argsJson: argsJson)
                    let watchId = NVJsonWire.int(a, key: "watchId", default: 0)
                    lock.lock()
                    var map = watchesByApp[appId] ?? [:]
                    if watchId == 0 {
                        for w in map.values {
                            w.stop()
                        }
                        map.removeAll()
                    } else if let w = map.removeValue(forKey: watchId) {
                        w.stop()
                    }
                    if map.isEmpty {
                        watchesByApp.removeValue(forKey: appId)
                    } else {
                        watchesByApp[appId] = map
                    }
                    lock.unlock()
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

    public static func unregister(app: NativeViewApp) {
        let appId = ObjectIdentifier(app)
        lock.lock()
        let map = watchesByApp.removeValue(forKey: appId)
        lock.unlock()
        map?.values.forEach { $0.stop() }
    }

    private static func allocWatchId() -> Int {
        lock.lock()
        defer { lock.unlock() }
        let id = nextWatchId
        nextWatchId += 1
        return id
    }

    private final class OneShotGetter: NSObject, CLLocationManagerDelegate {
        private let manager = CLLocationManager()
        private var completion: ((Result<CLLocation, Error>) -> Void)?
        private var holdSelf: OneShotGetter?

        override init() {
            super.init()
            manager.delegate = self
            manager.desiredAccuracy = kCLLocationAccuracyBest
        }

        func request(completion: @escaping (Result<CLLocation, Error>) -> Void) {
            self.completion = completion
            holdSelf = self
            manager.requestLocation()
        }

        func locationManager(_: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
            guard let done = completion else { return }
            completion = nil
            holdSelf = nil
            if let loc = locations.last {
                done(.success(loc))
            } else {
                done(.failure(NSError(domain: "nativeview.location", code: 1, userInfo: [NSLocalizedDescriptionKey: "location unavailable"])))
            }
        }

        func locationManager(_: CLLocationManager, didFailWithError error: Error) {
            guard let done = completion else { return }
            completion = nil
            holdSelf = nil
            done(.failure(error))
        }
    }

    private final class LocationWatch: NSObject, CLLocationManagerDelegate {
        private let manager = CLLocationManager()
        private weak var window: NativeViewWindow?

        init(window: NativeViewWindow) {
            self.window = window
            super.init()
            manager.delegate = self
            manager.desiredAccuracy = kCLLocationAccuracyBest
        }

        func start() {
            manager.startUpdatingLocation()
        }

        func stop() {
            manager.stopUpdatingLocation()
        }

        func locationManager(_: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
            guard let loc = locations.last, let w = window else { return }
            let speed = loc.speed >= 0 ? loc.speed : 0.0
            let obj: [String: Any] = [
                "lat": loc.coordinate.latitude,
                "lng": loc.coordinate.longitude,
                "accuracy": loc.horizontalAccuracy,
                "speed": speed,
            ]
            guard let json = NVJsonWire.jsonString(obj) else { return }
            w.emit(event: "location.update", json: json)
        }
    }
}
