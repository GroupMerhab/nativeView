package com.nativeview.bridge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.pm.PackageManager;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowApplication;

@RunWith(RobolectricTestRunner.class)
public class NVPermissionManagerTest {

    @Test
    public void hasPermission_nullSafe() {
        assertFalse(NVPermissionManager.hasPermission(null, "android.permission.CAMERA"));
        assertFalse(NVPermissionManager.hasPermission(null, null));
    }

    @Test
    public void deliverPermissionResult_unknownCodeReturnsFalse() {
        assertFalse(NVPermissionManager.deliverPermissionResult(0x1234, new String[0], new int[0]));
    }

    @Test
    public void checkThenRun_emptyPermissions_runsImmediately() {
        Context ctx = RuntimeEnvironment.getApplication();
        AtomicBoolean ran = new AtomicBoolean();
        NVPermissionManager.checkThenRun(
                ctx,
                null,
                new String[0],
                () -> ran.set(true),
                (c, m) -> {
                    throw new AssertionError("unexpected deny: " + c);
                });
        assertTrue(ran.get());
    }

    @Test
    public void checkThenRun_allGranted_runsWithoutActivity() {
        Application app = RuntimeEnvironment.getApplication();
        ShadowApplication shadow = Shadows.shadowOf(app);
        shadow.grantPermissions(android.Manifest.permission.CAMERA);
        AtomicBoolean ran = new AtomicBoolean();
        NVPermissionManager.checkThenRun(
                app,
                null,
                new String[] {android.Manifest.permission.CAMERA},
                () -> ran.set(true),
                (c, m) -> {
                    throw new AssertionError();
                });
        assertTrue(ran.get());
    }

    @Test
    public void checkThenRun_missingWithoutActivity_denies() {
        Context ctx = RuntimeEnvironment.getApplication();
        ShadowApplication shadow = Shadows.shadowOf((Application) ctx.getApplicationContext());
        shadow.denyPermissions(android.Manifest.permission.RECORD_AUDIO);
        AtomicReference<String> code = new AtomicReference<>();
        NVPermissionManager.checkThenRun(
                ctx,
                null,
                new String[] {android.Manifest.permission.RECORD_AUDIO},
                () -> {
                    throw new AssertionError();
                },
                (c, m) -> code.set(c));
        assertEquals("ERR_PERMISSION", code.get());
    }

    @Test
    public void checkThenRun_requestsThenDeliverGrants() {
        Application app = RuntimeEnvironment.getApplication();
        Shadows.shadowOf(app).denyPermissions(android.Manifest.permission.CAMERA);
        Activity real = Robolectric.buildActivity(Activity.class).create().get();
        Activity activity = Mockito.spy(real);
        AtomicInteger capturedCode = new AtomicInteger(-1);
        Mockito.doAnswer(
                        inv -> {
                            capturedCode.set(inv.getArgument(1, Integer.class));
                            return null;
                        })
                .when(activity)
                .requestPermissions(Mockito.any(), Mockito.anyInt());

        AtomicBoolean ran = new AtomicBoolean();
        NVPermissionManager.checkThenRun(
                app,
                activity,
                new String[] {android.Manifest.permission.CAMERA},
                () -> ran.set(true),
                (c, m) -> {
                    throw new AssertionError("denied: " + c + " " + m);
                });

        assertTrue(capturedCode.get() >= 0x7100);
        assertTrue(
                NVPermissionManager.deliverPermissionResult(
                        capturedCode.get(),
                        new String[] {android.Manifest.permission.CAMERA},
                        new int[] {PackageManager.PERMISSION_GRANTED}));
        assertTrue(ran.get());
    }
}
