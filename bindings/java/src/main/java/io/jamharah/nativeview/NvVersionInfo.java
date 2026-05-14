package io.jamharah.nativeview;

/** Mirrors C {@code NvVersionInfo} from {@code include/nv_util.h}. */
public final class NvVersionInfo {
  public final int major;
  public final int minor;
  public final int patch;
  public final String buildSha;

  public NvVersionInfo(int major, int minor, int patch, String buildSha) {
    this.major = major;
    this.minor = minor;
    this.patch = patch;
    this.buildSha = buildSha;
  }
}
