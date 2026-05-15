package com.nativeview.ops;

import static org.junit.Assert.assertEquals;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import org.junit.Test;

/**
 * Guardrail for the default Java bridge surface registered by {@link
 * com.nativeview.bridge.AndroidBridgeRegistry}. Update this list when adding or renaming {@code
 * app.handle} entries in ops classes.
 */
public class DefaultAndroidBridgeMethodKeysTest {

    private static final String[] EXPECTED = {
        "app.exit",
        "app.getInfo",
        "app.getPlatform",
        "app.getVersion",
        "biometric.authenticate",
        "biometric.isAvailable",
        "camera.pickImage",
        "camera.takePicture",
        "clipboard.clear",
        "clipboard.readText",
        "clipboard.writeText",
        "device.getInfo",
        "fs.deleteFile",
        "fs.exists",
        "fs.listDir",
        "fs.readFile",
        "fs.writeFile",
        "location.get",
        "location.unwatch",
        "location.watch",
        "network.getStatus",
        "network.onChange",
        "notification.cancel",
        "notification.cancelAll",
        "notification.show",
        "storage.clear",
        "storage.get",
        "storage.remove",
        "storage.set",
    };

    @Test
    public void defaultMethodKeys_areUnique() {
        Set<String> set = new HashSet<>(Arrays.asList(EXPECTED));
        assertEquals(EXPECTED.length, set.size());
    }

    @Test
    public void defaultMethodKeys_sortedForReview() {
        String[] copy = EXPECTED.clone();
        Arrays.sort(copy);
        assertEquals(Arrays.toString(copy), Arrays.toString(EXPECTED));
    }
}
