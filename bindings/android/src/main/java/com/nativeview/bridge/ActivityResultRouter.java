package com.nativeview.bridge;

import android.content.Intent;
import android.util.SparseArray;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Dispatches {@link android.app.Activity#onActivityResult(int, int, android.content.Intent)} to
 * bridge ops (camera capture, gallery pick) registered via {@link #submit(Callback)}.
 */
public final class ActivityResultRouter {

    private static final Object LOCK = new Object();
    private static final AtomicInteger NEXT_CODE = new AtomicInteger(0x7200);
    private static final SparseArray<Callback> PENDING = new SparseArray<>();

    private ActivityResultRouter() {}

    public interface Callback {
        void onResult(int resultCode, Intent data);
    }

    /** Allocates a request code and stores {@code callback} until {@link #deliver}. */
    public static int submit(Callback callback) {
        if (callback == null) {
            throw new IllegalArgumentException("callback");
        }
        int code = allocateRequestCode();
        synchronized (LOCK) {
            PENDING.put(code, callback);
        }
        return code;
    }

    /**
     * @return {@code true} when {@code requestCode} matched a pending bridge callback (consumed).
     */
    public static boolean deliver(int requestCode, int resultCode, Intent data) {
        Callback cb;
        synchronized (LOCK) {
            cb = PENDING.get(requestCode);
            if (cb == null) {
                return false;
            }
            PENDING.remove(requestCode);
        }
        cb.onResult(resultCode, data);
        return true;
    }

    private static int allocateRequestCode() {
        synchronized (LOCK) {
            int c = NEXT_CODE.getAndIncrement();
            if (c < 0x7200 || c > 0x7ffe) {
                NEXT_CODE.set(0x7201);
                return 0x7200;
            }
            return c;
        }
    }
}
