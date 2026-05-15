package com.nativeview;

/** Fired when the embedded document is ready (host should call this after load completes). */
@FunctionalInterface
public interface ReadyCallback {

    void onReady();
}
