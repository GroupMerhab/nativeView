package com.nativeview;

/**
 * Invoked when JavaScript posts a bridge message for this window (after native IPC routing for
 * push traffic, same contract as {@code nv_on_message}).
 */
@FunctionalInterface
public interface MessageCallback {

    /**
     * @param event event name from the wire object {@code e} field
     * @param json full wire JSON string (valid only for the duration of the callback; copy if
     *     needed)
     */
    void onMessage(String event, String json);
}
