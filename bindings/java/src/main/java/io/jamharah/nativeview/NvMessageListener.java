package io.jamharah.nativeview;

/**
 * Invoked from native when JavaScript posts a message. {@code event} and {@code json} are only
 * valid for the duration of the callback (mirror {@code nv_on_message} in {@code nv.h}).
 */
@FunctionalInterface
public interface NvMessageListener {
  void onMessage(long windowPtr, String event, String json);
}
