package com.nativeview.ops;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.MediaStore;
import android.util.Base64;
import com.nativeview.NativeViewApp;
import com.nativeview.ResolveCallback;
import com.nativeview.RejectCallback;
import com.nativeview.bridge.ActivityResultRouter;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import org.json.JSONException;
import org.json.JSONObject;

/** Camera capture and gallery pick (thumbnail capture uses system camera extras). */
public final class CameraOps {

    private CameraOps() {}

    public static void register(NativeViewApp app) {
        if (app == null) {
            return;
        }
        String[] cam = new String[] {android.Manifest.permission.CAMERA};
        app.handle(
                "camera.takePicture",
                cam,
                (argsJson, resolve, reject) -> {
                    Activity act = app.getHostActivity();
                    if (act == null) {
                        reject.reject("ERR_ACTIVITY", "host activity required");
                        return;
                    }
                    Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
                    if (intent.resolveActivity(act.getPackageManager()) == null) {
                        reject.reject("ERR_NOT_SUPPORTED", "no camera app");
                        return;
                    }
                    int code =
                            ActivityResultRouter.submit(
                                    (resultCode, data) ->
                                            onCaptureResult(resultCode, data, resolve, reject));
                    try {
                        act.startActivityForResult(intent, code);
                    } catch (RuntimeException ex) {
                        reject.reject("ERR_IO", ex.getMessage() != null ? ex.getMessage() : "startActivity failed");
                    }
                });
        app.handle(
                "camera.pickImage",
                null,
                (argsJson, resolve, reject) -> {
                    Activity act = app.getHostActivity();
                    if (act == null) {
                        reject.reject("ERR_ACTIVITY", "host activity required");
                        return;
                    }
                    Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
                    intent.setType("image/*");
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    Intent chooser = Intent.createChooser(intent, "Image");
                    int code =
                            ActivityResultRouter.submit(
                                    (resultCode, data) ->
                                            onPickResult(act, resultCode, data, resolve, reject));
                    try {
                        act.startActivityForResult(chooser, code);
                    } catch (RuntimeException ex) {
                        reject.reject("ERR_IO", ex.getMessage() != null ? ex.getMessage() : "startActivity failed");
                    }
                });
    }

    private static void onCaptureResult(
            int resultCode, Intent data, ResolveCallback resolve, RejectCallback reject) {
        if (resultCode != Activity.RESULT_OK) {
            reject.reject("ERR_CANCELLED", "cancelled");
            return;
        }
        if (data == null || data.getExtras() == null) {
            reject.reject("ERR_IO", "no image data");
            return;
        }
        Bitmap bmp = (Bitmap) data.getExtras().get("data");
        if (bmp == null) {
            reject.reject("ERR_IO", "no bitmap in result");
            return;
        }
        try {
            JSONObject o = bitmapToPayload(bmp, Bitmap.CompressFormat.JPEG, 90);
            resolve.resolve(o.toString());
        } catch (JSONException ex) {
            reject.reject("ERR_JSON", ex.getMessage() != null ? ex.getMessage() : "json error");
        }
    }

    private static void onPickResult(
            Activity act, int resultCode, Intent data, ResolveCallback resolve, RejectCallback reject) {
        if (resultCode != Activity.RESULT_OK || data == null) {
            reject.reject("ERR_CANCELLED", "cancelled");
            return;
        }
        Uri uri = data.getData();
        if (uri == null) {
            reject.reject("ERR_IO", "no uri");
            return;
        }
        try (InputStream in = act.getContentResolver().openInputStream(uri)) {
            if (in == null) {
                reject.reject("ERR_IO", "cannot open stream");
                return;
            }
            ByteArrayOutputStream buf = new ByteArrayOutputStream();
            byte[] tmp = new byte[8192];
            int n;
            while ((n = in.read(tmp)) != -1) {
                buf.write(tmp, 0, n);
            }
            String b64 = Base64.encodeToString(buf.toByteArray(), Base64.NO_WRAP);
            JSONObject o = new JSONObject();
            o.put("data", b64);
            o.put("format", "base64");
            resolve.resolve(o.toString());
        } catch (Exception ex) {
            reject.reject("ERR_IO", ex.getMessage() != null ? ex.getMessage() : "read failed");
        }
    }

    private static JSONObject bitmapToPayload(Bitmap bmp, Bitmap.CompressFormat fmt, int quality)
            throws JSONException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        bmp.compress(fmt, quality, baos);
        String b64 = Base64.encodeToString(baos.toByteArray(), Base64.NO_WRAP);
        JSONObject o = new JSONObject();
        o.put("data", b64);
        o.put("format", fmt == Bitmap.CompressFormat.PNG ? "png" : "jpeg");
        return o;
    }
}
