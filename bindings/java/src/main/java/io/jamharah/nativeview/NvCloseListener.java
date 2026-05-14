package io.jamharah.nativeview;

/** Invoked when the user closes the window ({@code nv_window_on_close}). */
@FunctionalInterface
public interface NvCloseListener {
  void onClose(long windowPtr);
}
