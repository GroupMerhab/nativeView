# iOS static `libnativeview.a` layout

Place merged **`libnativeview.a`** archives here (CMake + `libtool` output) before wrapping them in **`NativeView.xcframework`**:

```
libs/
├── arm64/
│   └── libnativeview.a              # iphoneos, arm64 (physical iPhone / iPad)
├── x86_64-simulator/
│   └── libnativeview.a              # iphonesimulator, x86_64 (Intel Mac simulator)
└── arm64-simulator/
    └── libnativeview.a              # iphonesimulator, arm64 (Apple Silicon simulator)
```

Populate with **`scripts/build_ios_static_libs.sh`** (runs three Xcode CMake builds and merges **`nv-platform-ios`** + **`nv-runtime`** + **`nv-ops`** + **`nv-ipc`** + **`nv-core`** per slice).

Then run **`scripts/create_nativeview_xcframework.sh`** (no flags). It **`lipo`**-merges the two simulator slices when both exist, stages **`include/`** + **`Headers/NativeViewC.h`** + **`module.modulemap`**, and runs:

```bash
xcodebuild -create-xcframework \
  -library libs/arm64/libnativeview.a -headers "<staging>/Headers" \
  -library "<fat-or-single-sim-lib>.a" -headers "<staging>/Headers" \
  -output NativeView.xcframework
```

For a quick link-only archive (no CMake), use **`scripts/create_nativeview_xcframework.sh --stub`**.
