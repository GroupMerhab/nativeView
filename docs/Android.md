# Android embedding — nativeview

Java/Kotlin library under **`bindings/android/`** (`com.nativeview`): System **WebView**, **`libnativeview.so`**, and Java bridge ops that mirror the desktop wire contract where applicable.

---

## What it is

- An **Android library (AAR)** you add as a Gradle dependency: JNI loads **`libnativeview`**, **`NativeViewApp`** owns routing, **`NativeViewWebView`** injects **`assets/nativeview.js`**, and **`NativeViewActivity`** (or your own host) ties lifecycle to the bridge.
- A **JSON wire** from JavaScript (`e`, `d`, optional `s` for request/response) with the same **`NativeView.invoke` / `NativeView.on`** client API as desktop after **`app.handshake`** succeeds on the native IPC path.
- **Built-in Java ops** for typical mobile capabilities (`device`, `storage`, `network`, `notification`, `app`, `camera`, `location`, `fs` under app sandbox dirs, `clipboard`, `biometric`) registered via **`NativeViewApp.registerDefaultAndroidHandlers()`**.

## What it is not

- **Not** the desktop **`io.jamharah.nativeview.NativeView`** JNI binding (different package, different entry: **Context** + **WebView** vs **`java.library.path`** + **`nv_*`**).
- **Not** a full replacement for every desktop **C op** on day one: methods with no Java handler and no native handler still fail at the IPC layer; Android-specific gaps are called out under [Ops reference](#ops-reference).
- **Not** a cross-platform UI toolkit: **`NativeViewAndroid`**, **`Orientation`**, and **`NativeViewActivity`** conveniences are **Android-only** (not in **`nv.h`**).

---

## Quick start

1. **Native libs:** Gradle builds **`libnativeview.so`** from the repo root **`CMakeLists.txt`** (`nativeview_shared`) when you **`implementation project(':nativeview')`** — install an **NDK** matching **`bindings/android/build.gradle`** **`ndkVersion`** via SDK Manager. Optional offline/script build: **`bindings/android/scripts/build_nativeview_so.sh`** (requires **`ANDROID_NDK_HOME`**) still copies into **`src/main/jniLibs/`** if you are not using Gradle’s CMake path.
2. In **settings.gradle**: `include ':nativeview'` → `project(':nativeview').projectDir = file('path/to/bindings/android')` (or publish the AAR and depend on coordinates).
3. **`implementation project(':nativeview')`** (or **`implementation '…:nativeview:…'`**).
4. **`public class MainActivity extends NativeViewActivity`** — implement **`getContentUrl()`** (or **`getContentHtml()`**), optionally **`onNativeViewAppCreated(NativeViewApp app)`** to call **`app.allow_origin("https://your.host")`** for non-**`file://`** pages.
5. **`AndroidManifest.xml`**: runtime permissions your JS will use (see [Ops reference](#ops-reference)); forward **`onRequestPermissionsResult`** and **`onActivityResult`** to **`NativeViewApp`**.

Full sample: **`examples/android_full_bridge/`**.

---

## Gradle setup

**Consumer `settings.gradle` (composite local project):**

```gradle
include ':nativeview'
project(':nativeview').projectDir = file('../../bindings/android') // adjust path

include ':app'
```

**Consumer `app/build.gradle`:**

```gradle
dependencies {
    implementation project(':nativeview')
}
```

**JDK 17–21** for the **Gradle daemon** is recommended (host JDKs newer than **21** can break older Gradle/Groovy—point **`JAVA_HOME`** at JDK **17** for `./gradlew`). **Android Gradle Plugin 8.5.2+** (16 KB zip alignment for uncompressed JNI in APKs/AABs per [16 KB page sizes](https://developer.android.com/guide/practices/page-sizes)), **`compileSdk` / `targetSdk` 34**, **`minSdk` 24** (aligned with the library). The library’s **`ndk`** block filters **`arm64-v8a`** and **`x86_64`** — match your shipped ABIs.

**Google Play / Android 15+ 16 KB page-size devices:** every shipped **`.so`** (including **`libnv-platform-android.so`** and **`libnativeview.so`**) is linked with **`-Wl,-z,max-page-size=16384`** / **`common-page-size=16384`**. The library manifest sets **`android:extractNativeLibs="false"`** so JNI stays uncompressed and **AGP 8.5.1+** can **16 KB** zip-align it per [16 KB page sizes](https://developer.android.com/guide/practices/page-sizes). Rebuild after native changes; optional standalone **`.so`** copies: **`bindings/android/scripts/build_nativeview_so.sh`**. Verify with **`zipalign -P 16`** / Studio APK checks as needed.

**Release artifacts (optional flat layout for CI or manual distribution):** run **`./gradlew assembleRelease packageNativeviewRelease`** (native **`.so`** files are taken from the built release **AAR**). Output: **`build/distributions/nativeview-android-0.1.0/nativeview-android.jar`** plus **`jni/<abi>/libnativeview.so`**. Full check (Android + desktop tests): **`bindings/android/scripts/package_release.sh`**. See **`bindings/android/CHANGELOG.md`** for version notes and tagging.

---

## Basic example (subclass host activity)

```java
public final class MainActivity extends NativeViewActivity {
    @Override
    protected String getContentUrl() {
        return "https://example.com/app/";
    }

    @Override
    protected void onNativeViewAppCreated(NativeViewApp app) {
        app.allow_origin("https://example.com");
    }
}
```

**Manifest (minimal):** launcher activity, **`INTERNET`** if you load remote URLs, plus permissions from the ops you call.

---

## API reference

Types are **`com.nativeview.*`** unless noted. **`void`** methods with no description return nothing meaningful; many are **NULL-safe** (early-return on null handles).

### `NativeViewApp`

| Method | Parameters | Returns | Notes |
|--------|------------|---------|--------|
| **`NativeViewApp(Context context)`** | Application or activity **context**; must not be null | — | Loads JNI, **`nativeInit`**. Throws **`IllegalArgumentException`** if context is null. |
| **`create_window(NativeViewConfig config)`** | Config or null (defaults used) | **`NativeViewWindow`** or null | Null after **`destroy()`** or native failure. |
| **`run()`** | — | — | **No-op** on Android (lifecycle replaces desktop **`nv_app_run`**). |
| **`quit()`** | — | — | **`nativeQuit`**; no-op unless a native loop is active. |
| **`destroy()`** | — | — | Unregisters Java bridge handlers, **`nativeDestroy`**. |
| **`getAppContext()`** | — | **`Context`** | Application context. |
| **`getHostActivity()`** | — | **`Activity`** or null | Set via **`setPermissionActivity`**. |
| **`registerDefaultAndroidHandlers()`** | — | — | Registers all built-in ops (see [Ops](#ops-reference)). |
| **`onActivityResult(int, int, Intent)`** | Standard activity result | **`boolean`** | **`true`** if consumed by **`ActivityResultRouter`** (camera / gallery). |
| **`allow_origin(String origin)`** | Scheme/host/port entry | — | Normalized; **`file://`** is pre-allowed. Invalid entries ignored. |
| **`allowedOriginsSnapshot()`** | — | **`Set<String>`** | Unmodifiable copy. |
| **`setPermissionActivity(Activity)`** | Activity or null | — | Weak ref; used for **`requestPermissions`** and **`app.exit`**. |
| **`onRequestPermissionsResult(int, String[], int[])`** | Standard callback | **`boolean`** | **`true`** if consumed by **`NVPermissionManager`**. |
| **`getRouter()`** | — | **`NVRouter`** | Advanced: **`unregister`**, **`snapshot`**. |
| **`handle(String method, String[] permissions, NVHandler handler)`** | Wire **`e`** key, optional runtime permission list, handler | — | Registers a Java bridge method. |
| **`deliverWebMessage(NativeViewWindow window, String wireJson)`** | Window, raw JSON | — | No WebView origin check; native dispatch after Java. |
| **`deliverWebMessage(WebView webView, NativeViewWindow window, String wireJson)`** | WebView (can be null), window, raw JSON | — | Origin gate when webView non-null; then Java handlers; then **`nativeDispatchWebMessage`**. |

**Wire errors produced in Java (reject / `rejectWire`):**

| Code | When |
|------|------|
| **`ERR_INVALID_JSON`** | Top-level wire is not a JSON object, or args extraction fails. |
| **`PERMISSION_DENIED`** | Document origin not in **`allow_origin`** list. |
| **`ERR_PERMISSION`** | Runtime permission missing / denied (**`NVPermissionManager`**). |
| **`ERR_HANDLER`** | Uncaught exception in **`NVHandler`**. |
| **`ERR_JSON`** | Resolve payload could not be turned into client JS. |

**Handler `reject` codes** (ops): include **`ERR_INVALID_ARG`**, **`ERR_IO`**, **`ERR_NOT_SUPPORTED`**, **`ERR_CANCELLED`**, **`ERR_ACTIVITY`**, **`ERR_INTERNAL`**, **`ERR_AUTH`**, **`ERR_JSON`**, **`ERR_INVALID_JSON`**, **`ERR_PERMISSION`** (notifications on API 33+), etc. (see each op below).

### `NativeViewActivity` *(abstract)*

| Method | Parameters | Returns | Notes |
|--------|------------|---------|--------|
| **`getContentUrl()`** | — | **`String`** | **Abstract**. Non-blank → **`load_url`** + **`allow_origin(url)`**. |
| **`getContentHtml()`** | — | **`String`** | Default `""` when URL empty. |
| **`getContentHtmlBaseUrl()`** | — | **`String`** | Default **`about:blank`**. |
| **`createWindowConfig()`** | — | **`NativeViewConfig`** | Override to tune creation. |
| **`onNativeViewAppCreated(NativeViewApp)`** | App | — | Hook for **`allow_origin`** for https hosts. |
| **`getNativeViewApp()`** / **`getNativeViewWindow()`** / **`getNativeViewWebView()`** | — | respective type | Protected accessors. |

Also implements **`NativeViewHost#dispatch`**: forwards native window opcodes to the **`WebView`** on the UI thread. Emits JS events **`app.resume`**, **`app.pause`**, **`app.destroy`**, **`app.deeplink`**, **`device.orientation`** (see class Javadoc). **Does not** override **`onActivityResult`** / **`onRequestPermissionsResult`** — override in your subclass and forward to **`NativeViewApp`**.

### `NativeViewWindow`

| Method | Parameters | Returns | Notes |
|--------|------------|---------|--------|
| **`getNativeHandle()`** | — | **`long`** | Opaque native address; **0** if invalid. |
| **`show()`** / **`hide()`** | — | — | Native window visibility hooks. |
| **`load_url(String url)`** | URL | — | No-op if null or handle 0. |
| **`load_html(String html, String baseUrl)`** | HTML, base | — | |
| **`eval_js(String js)`** | Script | — | |
| **`set_title(String title)`** | Title | — | |
| **`set_size(int width, int height)`** | Pixels | — | |
| **`on_message(MessageCallback)`** | Callback | — | Native **`nv_on_message`** path. |
| **`on_ready(ReadyCallback)`** / **`on_close(CloseCallback)`** | Callback | — | |
| **`emit(String event, String json)`** | Event name, JSON payload | — | **`event`** must be non-null; **`nv_send`** to page. |

### `NativeViewConfig`

Public fields: **`String title`**, **`int width`** (default 1024), **`int height`** (768), **`boolean resizable`** (true), **`boolean devtools`**.

### `NativeViewWebView` *(extends `WebView`)*

| Method | Parameters | Returns | Notes |
|--------|------------|---------|--------|
| **Constructors** | **`Context`**, optional **`AttributeSet`**, **`defStyleAttr`** | — | Enables JS, DOM storage, injects bridge on **`onPageFinished`**. |
| **`setOnReady` / `setOnClose` / `setNavigationListener` / `setPageErrorListener` / `setFileChooserHandler` / `setWebPermissionHandler`** | Callbacks | — | See nested **`NavigationListener`**, **`PageErrorListener`**, **`FileChooserHandler`**, **`WebPermissionHandler`**. |
| **`handleBackPressed()`** | — | **`boolean`** | History back or **`triggerOnClose()`**. |
| **`triggerOnClose()`** | — | — | Invokes close callback. |
| **`attachNativeBridge(NativeViewApp, NativeViewWindow)`** | App, window | — | Registers **`_NVBridge`** **`JavascriptInterface`**. |
| **`emitToJs(String event, String jsonPayload)`** | Event, JSON for **`d`** | — | Delegates to **`BridgeJavaScript.emitEvent`**. |
| **`wrapNativeViewJsForAndroid(String bridgeSource)`** | **`nativeview.js`** text | **`String`** | Static: base64 **`eval`** wrapper + **`__nv.send`** + **`NativeView._emit/_resolve/_reject`**. |
| **`createInjectJavascriptExpression(String script)`** | Source | **`String`** | Base64 IIFE only. |

### `NativeViewAndroid`

| Method | Parameters | Returns | Notes |
|--------|------------|---------|--------|
| **`attachHostActivity` / `detachHostActivity`** | **`Activity`** | — | Required for UI helpers; **`NativeViewActivity`** attaches in **`onResume`**. |
| **`initNativeLibrary()`** | — | — | **`NativeViewJNI.ensureLoaded()`**. |
| **`loadBridgeScript(Context)`** | Context | **`String`** | Reads **`assets/nativeview.js`**. Throws **`IllegalArgumentException`** if context null; **`IOException`** on I/O. |
| **`setStatusBarColor(String hex)`** | `#RGB` / `#RRGGBB` / `#AARRGGBB` | — | **`IllegalArgumentException`** if invalid. |
| **`setFullScreen(boolean)`** | enable | — | Immersive / restore. |
| **`setOrientation(Orientation)`** | Enum | — | |
| **`setKeepScreenOn(boolean)`** | enable | — | |
| **`hideKeyboard()`** / **`showKeyboard()`** | — | — | |

### `Orientation` *(enum)*

**`UNSPECIFIED`**, **`BEHIND`**, **`PORTRAIT`**, **`LANDSCAPE`**, **`SENSOR`**, **`NOSENSOR`**, **`SENSOR_LANDSCAPE`**, **`SENSOR_PORTRAIT`**, **`USER`**, **`FULL_SENSOR`**, **`LOCKED`**, **`FULL_USER`**. **`asActivityInfoValue()`** → int for **`Activity#setRequestedOrientation`**.

### `com.nativeview.jni.NativeViewJNI`

Constants **`OP_WINDOW_*`** mirror **`NativeViewJniOpcodes`**. **`ensureLoaded()`** triggers **`System.loadLibrary("nativeview")`**. Natives: **`nativeInit`**, **`nativeDestroy`**, **`nativeCreateWindow`**, **`nativeDestroyWindow`**, **`nativeLoadUrl`**, **`nativeLoadHtml`**, **`nativeEvalJs`**, **`nativeSetTitle`**, **`nativeSetSize`**, **`nativeShow`**, **`nativeHide`**, **`nativeEmit`**, **`nativeDispatchWebMessage`**, **`nativeOnMessage`**, **`nativeOnReady`**, **`nativeOnClose`**, **`nativeQuit`**.

### `com.nativeview.jni.NativeViewHost`

**`void dispatch(int op, long windowPtr, String s1, String s2)`** — implement on your **`Activity`** when not using **`NativeViewActivity`**.

### `com.nativeview.bridge.NVBridge`

**`NVBridge(NativeViewApp, NativeViewWindow, WebView)`** — **`@JavascriptInterface void post(String json)`**: posts **`app.deliverWebMessage(webView, window, json)`** on UI thread.

### `com.nativeview.bridge.NVRouter`

**`register`**, **`unregister`**, **`route`**, **`snapshot`** — see Javadoc for **`RouteResult`** (**`NOT_AVAILABLE`**, **`isAvailable()`**, **`getHandler()`**, **`getPermissions()`**).

### `com.nativeview.bridge.NVPermissionManager`

**`hasPermission(Context, String)`** — **`static`**. **`checkThenRun(Context, Activity, String[], Runnable, DenyCallback)`** — requests missing permissions on activity. **`deliverPermissionResult`** — static; returns **`true`** if the request code matched internal pending state.

### `com.nativeview.bridge.BridgeJavaScript`

**`emitEvent(WebView webView, String event, String jsonPayload)`** — push event to page (wire **`{e,d}`**).

### `com.nativeview.bridge.BridgeDispatchContext`

**`set`**, **`clear`**, **`app()`**, **`window()`** — thread-local context during **`NVHandler`** execution (used by **`network.onChange`** / **`location.watch`**).

### `WireJson`

**`parseWireObject`**, **`extractMethod`**, **`extractSeq`**, **`extractArgsJson`** — static helpers.

### Callback interfaces

**`NVHandler`**: **`void handle(String argsJson, ResolveCallback resolve, RejectCallback reject)`**.  
**`ResolveCallback.resolve(String json)`**, **`RejectCallback.reject(String code, String message)`**.  
**`MessageCallback`**, **`ReadyCallback`**, **`CloseCallback`** — single method each (see source Javadoc).

### Code example (custom handler)

```java
app.handle("myapp.ping", null, (argsJson, resolve, reject) -> {
    resolve.resolve("{\"ok\":true}");
});
```

```javascript
const r = await NativeView.invoke('myapp.ping', {});
```

---

## Ops reference

Registered by **`AndroidBridgeRegistry.registerAll`** → **`registerDefaultAndroidHandlers()`**. Unless noted, paths are **app-private** (no **`MANAGE_EXTERNAL_STORAGE`**).

### `device`

| Method | Android permissions | Response / notes |
|--------|---------------------|------------------|
| **`device.getInfo`** | *(none)* | **`platform`**, **`version`**, **`manufacturer`**, **`model`**, **`screenWidth`**, **`screenHeight`**, **`batteryLevel`** (nullable). Errors: **`ERR_JSON`**. |

### `storage` (`SharedPreferences`)

| Method | Permissions | Args / notes |
|--------|-------------|--------------|
| **`storage.set`** | *(none)* | **`key`** (required), optional **`value`**, optional **`prefs`** name (default **`nativeview`**). Segment must not contain **`..`**, **`/`**, **`\`**, NUL. |
| **`storage.get`** | *(none)* | **`key`**, optional **`type`**: **`string`** (default), **`boolean`**, **`int`**, **`long`**, **`float`**. Missing key → **`value: null`**. |
| **`storage.remove`** | *(none)* | **`key`**, optional **`prefs`**. |
| **`storage.clear`** | *(none)* | optional **`prefs`**. |

Reject codes: **`ERR_INVALID_ARG`**, **`ERR_INVALID_JSON`**.

### `network`

| Method | Permissions | Notes |
|--------|-------------|--------|
| **`network.getStatus`** | **`ACCESS_NETWORK_STATE`** | **`connected`**, **`type`** (`wifi` / `cellular` / `none` / `other`). |
| **`network.onChange`** | **`ACCESS_NETWORK_STATE`** | Subscribe; pushes **`network.change`** with same shape as **`getStatus`**. Requires **`BridgeDispatchContext`** window (**`ERR_INTERNAL`** if missing). |

### `notification`

| Method | Permissions | Notes |
|--------|-------------|--------|
| **`notification.show`** | **`POST_NOTIFICATIONS`** (API 33+) | **`id`**, **`title`**, **`body`**. |
| **`notification.cancel`** | *(none)* | **`id`**. |
| **`notification.cancelAll`** | *(none)* | Clears all posted by the app. |

Reject codes: **`ERR_IO`**, **`ERR_INVALID_JSON`**, **`ERR_PERMISSION`** (API 33+ if not granted).

### `app`

| Method | Permissions | Notes |
|--------|-------------|--------|
| **`app.getVersion`** | *(none)* | **`versionName`**, **`versionCode`**. |
| **`app.getPlatform`** | *(none)* | **`platform`: `"android"`**. |
| **`app.getInfo`** | *(none)* | **`packageName`**, **`buildType`**. |
| **`app.exit`** | *(none)* | Finishes host activity; needs **`setPermissionActivity`**. **`ERR_ACTIVITY`** if missing. |

### `camera`

| Method | Permissions | Notes |
|--------|-------------|--------|
| **`camera.takePicture`** | **`CAMERA`** | **`ACTION_IMAGE_CAPTURE`** thumbnail; **`ERR_ACTIVITY`**, **`ERR_NOT_SUPPORTED`**, **`ERR_CANCELLED`**, **`ERR_IO`**. Forward **`onActivityResult`** to **`NativeViewApp`**. |
| **`camera.pickImage`** | *(GET_CONTENT; storage per OEM)* | Gallery chooser; base64 payload. **`ERR_ACTIVITY`**, **`ERR_CANCELLED`**, **`ERR_IO`**. |

### `location` (`LocationManager`, no Play Services)

| Method | Permissions | Notes |
|--------|-------------|--------|
| **`location.get`** | **`ACCESS_FINE_LOCATION`** | Last known fix; **`ERR_IO`** if unavailable. |
| **`location.watch`** | **`ACCESS_FINE_LOCATION`** | **`minTime`**, **`minDistance`**; emits **`location.update`**. **`ERR_INTERNAL`** without window context. |
| **`location.unwatch`** | *(none)* | **`watchId`**. |

### `fs` (sandbox)

| Method | Permissions | Notes |
|--------|-------------|--------|
| **`fs.readFile`**, **`fs.writeFile`**, **`fs.deleteFile`**, **`fs.exists`**, **`fs.listDir`** | *(none)* for app dirs | **`path`**, optional **`root`**: **`files`** (default), **`cache`**, **`externalFiles`**. **`encoding`**: **`text`** or **`base64`**. Rejects path escape (**`SandboxPath`**). |

Reject codes: **`ERR_INVALID_ARG`**, **`ERR_IO`**, **`ERR_INVALID_JSON`**.

### `clipboard`

| Method | Permissions | Notes |
|--------|-------------|--------|
| **`clipboard.readText`**, **`clipboard.writeText`**, **`clipboard.clear`** | *(none)* | Text only. |

### `biometric` (AndroidX **`BiometricPrompt`**)

| Method | Permissions | Notes |
|--------|-------------|--------|
| **`biometric.isAvailable`** | *(none)* | **`available`** boolean; false if host is not **`FragmentActivity`**. |
| **`biometric.authenticate`** | *(none)* | **`title`**, **`subtitle`**, **`negativeButton`**. Host **must** be **`FragmentActivity`** (**`ERR_ACTIVITY`** otherwise). **`ERR_AUTH`**, **`success`** in JSON. |

**Platform note:** Use **`AppCompatActivity`** / **`FragmentActivity`** as the permission activity if you need biometrics; **`NativeViewActivity`** extends **`Activity`** and will not satisfy **`BiometricOps`**.

### JavaScript modules vs Android

The bundled **`nativeview.js`** exposes desktop-oriented helpers (e.g. **`NativeView.app.getName`**). Prefer **`NativeView.invoke('method.key', args)`** for Android-registered methods that are not wired in JS modules, or extend the JS build for your product.

---

## Migration from Java desktop (`bindings/java`)

| Desktop | Android |
|---------|---------|
| **`io.jamharah.nativeview.NativeView`** + **`java.library.path`** + **`nativeview_jni`** | **`com.nativeview.NativeViewApp`** + **`libnativeview.so`** (Gradle **CMake** / packaged JNI) + **WebView** host |
| **`NvWindow` / `nv_window_*` via JNI`** | **`NativeViewWindow`** + **`NativeViewHost.dispatch`** driving **`WebView`** |
| **Ops in C** | **Default mobile ops in Java** + **remaining wire** handled by **native IPC** after Java |

**One-line dependency difference (Gradle):**

```gradle
// Before (desktop JAR + JNI path to nativeview_jni + nativeview)
implementation files('libs/nativeview-java.jar')

// After (Android library project or AAR + jniLibs)
implementation project(':nativeview')
```

Keep **`NativeView.invoke('app.handshake', …)`** semantics: handshake is served by **native** **`nv_ipc_dispatch`** after Java handler lookup misses.

---

## Tests

- **Unit:** `cd bindings/android && ./gradlew test`  
- **Instrumented:** `./gradlew connectedAndroidTest` (requires a successful **debug** build with native **`.so`** for the emulator/device ABI).  
- **Wire parity:** shared **`bindings/parity-tests`** (`io.jamharah.nativeview.parity.WireContractParityTest`).

---

## See also

- **`docs/Java.md`** — desktop JNI  
- **`docs/project_structure.md`** — repository layout  
- **`bindings/android/scripts/build_nativeview_so.sh`** — native **`.so`** build
