package com.nativeview;

/**
 * Handles a bridge method invoked from JavaScript (request/response style). Use with {@link
 * NativeViewApp#handle}.
 */
@FunctionalInterface
public interface NVHandler {

    void handle(String argsJson, ResolveCallback resolve, RejectCallback reject);
}
