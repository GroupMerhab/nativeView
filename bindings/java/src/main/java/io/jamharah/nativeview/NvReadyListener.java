package io.jamharah.nativeview;

/** Invoked when the webview document is ready ({@code nv_on_ready}). */
@FunctionalInterface
public interface NvReadyListener {
  void onReady(long windowPtr);
}
