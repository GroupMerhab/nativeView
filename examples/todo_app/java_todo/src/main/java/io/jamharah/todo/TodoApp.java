/* SPDX-License-Identifier: Apache-2.0 */
package io.jamharah.todo;

import io.jamharah.nativeview.NativeView;
import io.jamharah.nativeview.NvCloseListener;
import io.jamharah.nativeview.NvMessageListener;
import io.jamharah.nativeview.NvReadyListener;
import io.jamharah.nativeview.NvWindowCfg;
import java.io.IOException;
import java.io.InputStream;

/**
 * Todo app: Vue UI + SQLite + nativeview (Java port of {@code nim_todo/todo_app.nim}).
 */
public final class TodoApp {

  private TodoApp() {}

  private static byte[] readEmbeddedUi() throws IOException {
    try (InputStream in = TodoApp.class.getResourceAsStream("/todo_ui.html")) {
      if (in == null) {
        throw new IOException("missing classpath resource /todo_ui.html (run build_shared.sh)");
      }
      return in.readAllBytes();
    }
  }

  public static void main(String[] args) {
    String dbPath =
        args != null && args.length >= 1 && args[0] != null && !args[0].isEmpty()
            ? args[0]
            : "todo_app.db";

    NativeView.loadLibrary();

    TodoStore store = TodoStore.storeOpen(dbPath);
    if (store == null) {
      System.err.println("[todo_app] todo_store_open failed for " + dbPath);
      System.exit(1);
    }

    long app = NativeView.nvAppCreate();
    if (app == 0) {
      System.err.println("[todo_app] nv_app_create failed");
      TodoStore.storeClose(store);
      System.exit(1);
    }

    NvWindowCfg cfg = new NvWindowCfg();
    cfg.title = "Todo App";
    cfg.width = 960;
    cfg.height = 720;
    cfg.minWidth = 0;
    cfg.minHeight = 0;
    cfg.resizable = 1;
    cfg.frameless = 0;
    cfg.transparent = 0;
    cfg.devtools = 1;
    cfg.modal = 0;

    long win = NativeView.nvWindowCreate(app, cfg);
    if (win == 0) {
      System.err.println("[todo_app] nv_window_create failed");
      TodoStore.storeClose(store);
      NativeView.nvAppDestroy(app);
      System.exit(1);
    }

    final long appFinal = app;

    NativeView.nvOnReady(
        win,
        new NvReadyListener() {
          @Override
          public void onReady(long windowPtr) {
            System.err.println("[todo_app] webview ready\n");
          }
        });

    NativeView.nvOnMessage(
        win,
        new NvMessageListener() {
          @Override
          public void onMessage(long windowPtr, String event, String json) {
            if (!"todo".equals(event) || json == null) {
              return;
            }
            TodoBridge.bridgeHandleWire(
                store,
                json,
                (ev, j) -> NativeView.nvSend(windowPtr, ev, j));
          }
        });

    NativeView.nvWindowOnClose(
        win,
        new NvCloseListener() {
          @Override
          public void onClose(long windowPtr) {
            NativeView.nvAppQuit(appFinal);
          }
        });

    try {
      byte[] html = readEmbeddedUi();
      NativeView.nvLoadHtmlRef(win, html, html.length, "about:blank");
    } catch (IOException e) {
      System.err.println("[todo_app] " + e.getMessage());
      TodoStore.storeClose(store);
      NativeView.nvWindowDestroy(win);
      NativeView.nvAppDestroy(app);
      System.exit(1);
    }

    NativeView.nvWindowShow(win);
    NativeView.nvAppRun(app);

    TodoStore.storeClose(store);
    NativeView.nvAppDestroy(app);
  }
}
