package io.jamharah.nativeview;

/**
 * Window creation options; maps to {@code nv_window_cfg_t}. {@code title} must stay meaningful
 * until {@link NativeView#nvWindowCreate} returns (same rule as C {@code nv.h}).
 */
public final class NvWindowCfg {
  public String title;
  public int width = 800;
  public int height = 600;
  public int minWidth;
  public int minHeight;
  public int resizable = 1;
  public int frameless;
  public int transparent;
  public int devtools;
  public int modal;
}
