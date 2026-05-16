// SPDX-License-Identifier: Apache-2.0

import UIKit
import NativeViewIOS

/// Hosts navigation actions for the example (switch between bundled `loadUrl` and `loadHtml` demos).
final class ExampleNavigationCoordinator: NSObject {
    private weak var nvWindow: NativeViewWindow?

    func attach(nvWindow: NativeViewWindow) {
        self.nvWindow = nvWindow
        let bundled = UIAction(title: "Bundled index.html (loadUrl)", image: nil) { [weak self] _ in
            self?.loadBundledIndex()
        }
        let inline = UIAction(title: "Inline HTML (loadHtml)", image: nil) { [weak self] _ in
            self?.loadInlineHtmlDemo()
        }
        let menu = UIMenu(title: "Reload demo", children: [bundled, inline])
        nvWindow.viewController().navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Load…",
            menu: menu
        )
    }

    private func loadBundledIndex() {
        guard let url = Bundle.main.url(forResource: "index", withExtension: "html") else {
            print("[Example] Missing index.html in bundle")
            return
        }
        nvWindow?.loadUrl(url.absoluteString)
    }

    /// Uses a `file://` base URL so the document origin stays allow-listed like a normal app resource page.
    private func loadInlineHtmlDemo() {
        let base = URL(fileURLWithPath: Bundle.main.bundlePath).absoluteString
        let html = """
        <!DOCTYPE html>
        <html lang="en">
        <head>
          <meta charset="utf-8">
          <meta name="viewport" content="width=device-width, initial-scale=1">
          <title>Inline loadHtml</title>
          <style>
            body { font-family: system-ui; margin: 16px; background: #1c1c1e; color: #f2f2f7; }
            code { background: #2c2c2e; padding: 2px 6px; border-radius: 4px; }
            pre { background: #000; padding: 12px; border-radius: 8px; overflow: auto; }
          </style>
        </head>
        <body>
          <p>This screen was loaded with <strong>NativeViewWindow.loadHtml(_:baseUrl:)</strong> using a
          <code>file://</code> base derived from <code>Bundle.main.bundlePath</code> so the bridge origin
          matches the default <code>file://</code> allow list.</p>
          <p><button type="button" id="echoBtn">Invoke Swift handler <code>example.echo</code></button></p>
          <pre id="out"></pre>
          <script>
            function log(msg) {
              var el = document.getElementById('out');
              el.textContent += msg + '\\n';
            }
            document.getElementById('echoBtn').onclick = function() {
              NativeView.invoke('example.echo', { message: 'Hello from inline HTML' })
                .then(function(r) { log('OK: ' + JSON.stringify(r)); })
                .catch(function(e) { log('ERR: ' + (e && e.message ? e.message : String(e))); });
            };
          </script>
        </body>
        </html>
        """
        nvWindow?.loadHtml(html, baseUrl: base)
    }
}
