package com.nativeview;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;

import android.content.pm.ActivityInfo;
import org.junit.Test;

public class NativeViewAndroidTest {

    @Test
    public void parseArgbFromHex_rrggbb() {
        assertEquals(0xFF112233, NativeViewAndroid.parseArgbFromHex("#112233"));
        assertEquals(0xFFaBcDeF, NativeViewAndroid.parseArgbFromHex("abcdef"));
    }

    @Test
    public void parseArgbFromHex_rgb_expands() {
        assertEquals(0xFF00FFEE, NativeViewAndroid.parseArgbFromHex("#0fe"));
    }

    @Test
    public void parseArgbFromHex_aarrggbb() {
        assertEquals(0x80112233, NativeViewAndroid.parseArgbFromHex("#80112233"));
    }

    @Test
    public void parseArgbFromHex_rejects_bad() {
        assertThrows(IllegalArgumentException.class, () -> NativeViewAndroid.parseArgbFromHex(null));
        assertThrows(IllegalArgumentException.class, () -> NativeViewAndroid.parseArgbFromHex(""));
        assertThrows(IllegalArgumentException.class, () -> NativeViewAndroid.parseArgbFromHex("#gg0000"));
        assertThrows(IllegalArgumentException.class, () -> NativeViewAndroid.parseArgbFromHex("#12"));
    }

    @Test
    public void orientation_maps_to_activity_info() {
        assertEquals(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, Orientation.PORTRAIT.asActivityInfoValue());
        assertEquals(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, Orientation.LANDSCAPE.asActivityInfoValue());
        assertEquals(ActivityInfo.SCREEN_ORIENTATION_SENSOR, Orientation.SENSOR.asActivityInfoValue());
    }
}
