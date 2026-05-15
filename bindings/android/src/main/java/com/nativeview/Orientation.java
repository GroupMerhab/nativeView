package com.nativeview;

import android.content.pm.ActivityInfo;

/**
 * Android-only screen orientation presets for {@link NativeViewAndroid#setOrientation(Orientation)}.
 *
 * <p>These values are <strong>not</strong> part of the nativeview C core or the cross-platform
 * JavaScript API; they map directly to {@link Activity#setRequestedOrientation(int)}.
 */
public enum Orientation {
    /** {@link ActivityInfo#SCREEN_ORIENTATION_UNSPECIFIED} */
    UNSPECIFIED(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_BEHIND} */
    BEHIND(ActivityInfo.SCREEN_ORIENTATION_BEHIND),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_PORTRAIT} */
    PORTRAIT(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_LANDSCAPE} */
    LANDSCAPE(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_SENSOR} */
    SENSOR(ActivityInfo.SCREEN_ORIENTATION_SENSOR),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_NOSENSOR} */
    NOSENSOR(ActivityInfo.SCREEN_ORIENTATION_NOSENSOR),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_SENSOR_LANDSCAPE} */
    SENSOR_LANDSCAPE(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_SENSOR_PORTRAIT} */
    SENSOR_PORTRAIT(ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_USER} */
    USER(ActivityInfo.SCREEN_ORIENTATION_USER),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_FULL_SENSOR} */
    FULL_SENSOR(ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_LOCKED} (API 18+) */
    LOCKED(ActivityInfo.SCREEN_ORIENTATION_LOCKED),
    /** {@link ActivityInfo#SCREEN_ORIENTATION_FULL_USER} (API 18+) */
    FULL_USER(ActivityInfo.SCREEN_ORIENTATION_FULL_USER);

    private final int screenOrientationConstant;

    Orientation(int screenOrientationConstant) {
        this.screenOrientationConstant = screenOrientationConstant;
    }

    /** Value accepted by {@link android.app.Activity#setRequestedOrientation(int)}. */
    public int asActivityInfoValue() {
        return screenOrientationConstant;
    }
}
