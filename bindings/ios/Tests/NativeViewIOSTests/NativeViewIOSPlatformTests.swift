// SPDX-License-Identifier: Apache-2.0

import UIKit
import XCTest
@testable import NativeViewIOS

final class NativeViewIOSPlatformTests: XCTestCase {
    func testSetKeepScreenOnDoesNotCrash() {
        NativeViewIOS.setKeepScreenOn(true)
        NativeViewIOS.setKeepScreenOn(false)
    }

    func testHideKeyboardDoesNotCrash() {
        NativeViewIOS.hideKeyboard()
    }

    func testSwipeBackForceOffCanBeToggled() {
        NativeViewIOS.disableSwipeBackGesture()
        XCTAssertTrue(NativeViewIOS.isSwipeBackGestureForcedOff())
        NativeViewIOS.enableSwipeBackGesture()
        XCTAssertFalse(NativeViewIOS.isSwipeBackGestureForcedOff())
    }

    func testSafeAreaPreferenceDefaultsAndUpdates() {
        NativeViewIOS.setSafeAreaEnabled(true)
        XCTAssertTrue(NativeViewIOS.isSafeAreaLayoutEnabled())
        NativeViewIOS.setSafeAreaEnabled(false)
        XCTAssertFalse(NativeViewIOS.isSafeAreaLayoutEnabled())
        NativeViewIOS.setSafeAreaEnabled(true)
        XCTAssertTrue(NativeViewIOS.isSafeAreaLayoutEnabled())
    }
}
