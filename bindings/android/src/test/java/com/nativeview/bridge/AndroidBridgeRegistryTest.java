package com.nativeview.bridge;

import org.junit.Test;

public class AndroidBridgeRegistryTest {

    @Test
    public void registerAll_nullDoesNotThrow() {
        AndroidBridgeRegistry.registerAll(null);
        AndroidBridgeRegistry.unregisterAll(null);
    }
}
