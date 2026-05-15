package com.nativeview.ops;

import androidx.biometric.BiometricManager;
import androidx.biometric.BiometricPrompt;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.FragmentActivity;
import com.nativeview.NativeViewApp;
import com.nativeview.bridge.BridgeArgs;
import java.util.concurrent.Executor;
import org.json.JSONException;
import org.json.JSONObject;

/** Biometric authentication via AndroidX BiometricPrompt (host activity should extend {@link
 * FragmentActivity}). */
public final class BiometricOps {

    private BiometricOps() {}

    public static void register(NativeViewApp app) {
        if (app == null) {
            return;
        }
        app.handle(
                "biometric.isAvailable",
                null,
                (argsJson, resolve, reject) -> {
                    try {
                        FragmentActivity fa = asFragmentActivity(app.getHostActivity());
                        if (fa == null) {
                            JSONObject o = new JSONObject();
                            o.put("available", false);
                            resolve.resolve(o.toString());
                            return;
                        }
                        BiometricManager bm = BiometricManager.from(fa);
                        int can = bm.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_WEAK);
                        boolean available = can == BiometricManager.BIOMETRIC_SUCCESS;
                        JSONObject o = new JSONObject();
                        o.put("available", available);
                        resolve.resolve(o.toString());
                    } catch (JSONException ex) {
                        reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
                    }
                });
        app.handle(
                "biometric.authenticate",
                null,
                (argsJson, resolve, reject) -> {
                    FragmentActivity fa = asFragmentActivity(app.getHostActivity());
                    if (fa == null) {
                        reject.reject("ERR_ACTIVITY", "FragmentActivity host required for biometric");
                        return;
                    }
                    final JSONObject a;
                    try {
                        a = BridgeArgs.requireObject(argsJson);
                    } catch (JSONException ex) {
                        reject.reject("ERR_INVALID_JSON", ex.getMessage() != null ? ex.getMessage() : "parse error");
                        return;
                    }
                    String title = a.optString("title", "Authenticate");
                    String subtitle = a.optString("subtitle", "");
                    String negative = a.optString("negativeButton", "Cancel");
                    Executor exec = ContextCompat.getMainExecutor(fa);
                    BiometricPrompt prompt =
                            new BiometricPrompt(
                                    fa,
                                    exec,
                                    new BiometricPrompt.AuthenticationCallback() {
                                        @Override
                                        public void onAuthenticationSucceeded(
                                                BiometricPrompt.AuthenticationResult result) {
                                            try {
                                                JSONObject o = new JSONObject();
                                                o.put("success", true);
                                                resolve.resolve(o.toString());
                                            } catch (JSONException ex) {
                                                reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
                                            }
                                        }

                                        @Override
                                        public void onAuthenticationError(int errorCode, CharSequence errString) {
                                            reject.reject(
                                                    "ERR_AUTH",
                                                    errString != null ? errString.toString() : "auth error");
                                        }

                                        @Override
                                        public void onAuthenticationFailed() {
                                            try {
                                                JSONObject o = new JSONObject();
                                                o.put("success", false);
                                                resolve.resolve(o.toString());
                                            } catch (JSONException ex) {
                                                reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
                                            }
                                        }
                                    });
                    BiometricPrompt.PromptInfo.Builder b =
                            new BiometricPrompt.PromptInfo.Builder()
                                    .setTitle(title)
                                    .setNegativeButtonText(negative);
                    if (!subtitle.isEmpty()) {
                        b.setSubtitle(subtitle);
                    }
                    try {
                        prompt.authenticate(b.build());
                    } catch (RuntimeException ex) {
                        reject.reject("ERR_IO", ex.getMessage() != null ? ex.getMessage() : "authenticate failed");
                    }
                });
    }

    private static FragmentActivity asFragmentActivity(android.app.Activity activity) {
        if (activity instanceof FragmentActivity) {
            return (FragmentActivity) activity;
        }
        return null;
    }
}
