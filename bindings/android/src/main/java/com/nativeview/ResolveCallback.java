package com.nativeview;

/** Completes a bridge request with a JSON result value (object/array/primitive string). */
@FunctionalInterface
public interface ResolveCallback {

    /**
     * @param json JSON text for the {@code d} field of the reply (for example {@code "{}"} or
     *     {@code "null"}).
     */
    void resolve(String json);
}
