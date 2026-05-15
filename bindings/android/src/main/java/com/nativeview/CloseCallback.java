package com.nativeview;

/** Fired when the window is closing from the native side. */
@FunctionalInterface
public interface CloseCallback {

    void onClose();
}
