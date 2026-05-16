// SPDX-License-Identifier: Apache-2.0

import UIKit

/// Root view controller hosting a ``NativeViewWebView`` and coordinating bridge callbacks.
public final class NativeViewViewController: UIViewController {
    public private(set) var webView: NativeViewWebView
    public var bridge: NVBridge?
    weak var hostWindow: NativeViewWindow?

    private let windowConfig: NativeViewConfig
    private var webViewLayoutConstraints: [NSLayoutConstraint] = []
    private var safeAreaPreferenceObserver: NSObjectProtocol?

    /// When `true`, the entire host view is hidden (see ``NativeViewWindow/hide()``).
    /// Defaults to `false` so embedding the view controller in a window shows content without an extra ``NativeViewWindow/show()`` call.
    var hostViewIsHidden: Bool = false {
        didSet { view.isHidden = hostViewIsHidden }
    }

    /// Designated initializer: embeds the given ``NativeViewWebView`` (already configured with bridge scripts).
    public init(webView: NativeViewWebView, bridge: NVBridge? = nil, config: NativeViewConfig = NativeViewConfig()) {
        self.webView = webView
        self.bridge = bridge
        self.windowConfig = config
        super.init(nibName: nil, bundle: nil)
        webView.bridgeOwner = bridge
    }

    /// Internal factory for ``NativeViewWindow``: wires ``NVBridge`` as the `_NVBridge` script handler.
    internal convenience init(bridge: NVBridge, config: NativeViewConfig = NativeViewConfig()) {
        let wv = NativeViewWebView(windowConfig: config, bridgeHandler: bridge)
        self.init(webView: wv, bridge: bridge, config: config)
    }

    @available(*, unavailable)
    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override public func loadView() {
        view = UIView()
        view.backgroundColor = .systemBackground
        view.isHidden = hostViewIsHidden
    }

    deinit {
        if let safeAreaPreferenceObserver {
            NotificationCenter.default.removeObserver(safeAreaPreferenceObserver)
        }
    }

    override public func viewDidLoad() {
        super.viewDidLoad()
        setupWebView()
        safeAreaPreferenceObserver = NotificationCenter.default.addObserver(
            forName: .nativeViewIOSSafeAreaPreferenceDidChange,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            guard let self else { return }
            self.applyWebViewLayout(useSafeArea: NativeViewIOS.isSafeAreaLayoutEnabled())
        }
        title = windowConfig.title.isEmpty ? nil : windowConfig.title
        preferredContentSize = CGSize(width: windowConfig.width, height: windowConfig.height)
    }

    override public var preferredStatusBarStyle: UIStatusBarStyle {
        NativeViewIOS.preferredStatusBarStyleForHosting()
    }

    override public var prefersStatusBarHidden: Bool {
        NativeViewIOS.prefersStatusBarHiddenForHosting()
    }

    override public var supportedInterfaceOrientations: UIInterfaceOrientationMask {
        NativeViewIOS.supportedInterfaceOrientationsForHosting()
    }

    override public func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        webView.refreshPopGestureEnabled()
    }

    private func setupWebView() {
        webView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(webView)
        applyWebViewLayout(useSafeArea: NativeViewIOS.isSafeAreaLayoutEnabled())
    }

    private func applyWebViewLayout(useSafeArea: Bool) {
        NSLayoutConstraint.deactivate(webViewLayoutConstraints)
        webViewLayoutConstraints.removeAll()

        let top = useSafeArea ? view.safeAreaLayoutGuide.topAnchor : view.topAnchor
        let bottom = useSafeArea ? view.safeAreaLayoutGuide.bottomAnchor : view.bottomAnchor
        let leading = useSafeArea ? view.safeAreaLayoutGuide.leadingAnchor : view.leadingAnchor
        let trailing = useSafeArea ? view.safeAreaLayoutGuide.trailingAnchor : view.trailingAnchor

        webViewLayoutConstraints = [
            webView.topAnchor.constraint(equalTo: top),
            webView.bottomAnchor.constraint(equalTo: bottom),
            webView.leadingAnchor.constraint(equalTo: leading),
            webView.trailingAnchor.constraint(equalTo: trailing),
        ]
        NSLayoutConstraint.activate(webViewLayoutConstraints)
    }

    func detachWebView() {
        webView.prepareForTeardown()
        webView.removeFromSuperview()
    }
}
