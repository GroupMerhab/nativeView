package com.nativeview;

/** Completes a bridge request with an error. */
@FunctionalInterface
public interface RejectCallback {

    void reject(String code, String message);
}
