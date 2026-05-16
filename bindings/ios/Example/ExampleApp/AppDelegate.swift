// SPDX-License-Identifier: Apache-2.0

import UIKit
import NativeViewIOS

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?
    var app: NativeViewApp?

    private var nvWindow: NativeViewWindow?
    private var navCoordinator: ExampleNavigationCoordinator?

    func application(
        _ application: UIApplication,
        didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
    ) -> Bool {
        let nvApp = NativeViewApp()
        app = nvApp
        nvApp.registerDefaultIOSHandlers()
        registerExampleEcho(on: nvApp)

        let config = NativeViewConfig(
            title: "Example",
            width: 390,
            height: 844
        )
        let windowHost = nvApp.createWindow(config: config)
        nvWindow = windowHost

        windowHost.onReady {
            print("[Example] WebView ready")
        }

        windowHost.onMessage { event, json in
            print("[Example] JS → native push (seq 0): \(event) \(json)")
        }

        if let url = launchOptions?[UIApplication.LaunchOptionsKey.url] as? URL {
            nvApp.handleDeepLink(url: url)
        }

        if let indexURL = Bundle.main.url(forResource: "index", withExtension: "html") {
            windowHost.loadUrl(indexURL.absoluteString)
        } else {
            let base = URL(fileURLWithPath: Bundle.main.bundlePath).absoluteString
            windowHost.loadHtml(
                "<p>Add <code>index.html</code> to the app target’s Copy Bundle Resources.</p>",
                baseUrl: base
            )
        }

        let root = windowHost.viewController()
        let nav = UINavigationController(rootViewController: root)
        nav.navigationBar.prefersLargeTitles = false

        let coord = ExampleNavigationCoordinator()
        coord.attach(nvWindow: windowHost)
        navCoordinator = coord

        let win = UIWindow(frame: UIScreen.main.bounds)
        win.rootViewController = nav
        win.makeKeyAndVisible()
        window = win

        return true
    }

    func application(_ application: UIApplication, open url: URL, options: [UIApplication.OpenURLOptionsKey: Any] = [:]) -> Bool {
        print("[Example] Deep link URL: \(url.absoluteString)")
        app?.handleDeepLink(url: url)
        return true
    }

    func applicationWillResignActive(_ application: UIApplication) {
        print("[Example] UIKit applicationWillResignActive")
    }

    func applicationDidBecomeActive(_ application: UIApplication) {
        print("[Example] UIKit applicationDidBecomeActive")
    }

    private func registerExampleEcho(on nvApp: NativeViewApp) {
        nvApp.handle("example.echo", permissions: []) { _, argsJson, resolve, reject in
            guard let data = argsJson.data(using: .utf8),
                  let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
            else {
                reject("ERR_INVALID_JSON", "bad args")
                return
            }
            let msg = obj["message"] as? String ?? ""
            let reply: [String: Any] = ["echo": msg, "from": "Swift"]
            guard JSONSerialization.isValidJSONObject(reply),
                  let outData = try? JSONSerialization.data(withJSONObject: reply),
                  let outStr = String(data: outData, encoding: .utf8)
            else {
                reject("ERR_JSON", "encode failed")
                return
            }
            resolve(outStr)
        }
    }
}
