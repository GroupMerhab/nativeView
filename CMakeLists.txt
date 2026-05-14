cmake_minimum_required(VERSION 3.16)
project(nativeview C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Generate version header from versions.json before any modules are built
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVersion.cmake)

# Linux: expose POSIX prototypes (clock_gettime, etc.) on glibc 2.34+.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_compile_definitions(_GNU_SOURCE)
endif()

option(NV_ENFORCE_FILE_LIMITS "Fail configure if new/modified sources exceed repo line limits" ON)
if(NV_ENFORCE_FILE_LIMITS)
  execute_process(
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_file_lengths.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE _nv_file_len_rc
    OUTPUT_VARIABLE _nv_file_len_out
    ERROR_VARIABLE _nv_file_len_err
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )
  if(NOT _nv_file_len_rc EQUAL 0)
    message(FATAL_ERROR "File length check failed:\n${_nv_file_len_err}\n(set NV_ENFORCE_FILE_LIMITS=OFF to bypass)")
  endif()
endif()

# =============================================================================
# Shared library option (declared early so subdirectories inherit PIC flag)
# =============================================================================

option(NV_BUILD_SHARED "Build nativeview as a shared library (.dll/.so/.dylib)" OFF)

# Static archives linked into a shared lib must be position-independent on ELF
# platforms.  Set this before any add_subdirectory() calls so every module
# picks it up.
if((NV_BUILD_SHARED AND UNIX AND NOT APPLE) OR CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# =============================================================================
# Build Type Configuration
# =============================================================================

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Debug/Release flags (GCC/Clang; MSVC uses its own defaults)
if(NOT MSVC)
  set(CMAKE_C_FLAGS_DEBUG "-g -DNV_DEBUG -O0 -Wall -Wextra")
  set(CMAKE_C_FLAGS_RELEASE "-Oz -DNDEBUG -ffunction-sections -fdata-sections")
else()
  set(CMAKE_C_FLAGS_DEBUG "/Zi /Od /DNV_DEBUG")
  set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
endif()

# Platform-specific linker flags for Release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,-dead_strip")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-dead_strip")
  elseif(UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
  endif()
endif()

# =============================================================================
# Module Build
#
# Each module is an independent CMake project under modules/.
# Dependency order (DAG, no cycles):
#
#   nv-core → nv-ipc → nv-ops → nv-runtime → nv-platform-{mac,win,linux}
#                    └→ nv-ui → nv-designer
#   nv-sdk (meta: nativeview + nv-ui)
#
# NV_FULL_LINK_TARGET: the target tests should link against to get the full
# stack including the platform backend. Passed into each module as a cache var.
# =============================================================================

# Optional UI stack (nv-ui, nv-designer, nv-sdk).
# Some embedders only need the core nativeview windowing/runtime and do not
# ship a Vue bundle. Allow them to disable the UI modules to avoid a hard Vue
# dependency during configuration.
option(NV_BUILD_UI "Build nv-ui and dependent modules (requires Vue bundle)" ON)

# Determine platform module name before subdirectories are processed.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NvDetectWindowsPeToolchain.cmake)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(NV_PLATFORM_MODULE nv-platform-ios)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(NV_PLATFORM_MODULE nv-platform-android)
elseif(APPLE)
  set(NV_PLATFORM_MODULE nv-platform-mac)
elseif(NV_WINDOWS_PE_TOOLCHAIN)
  set(NV_PLATFORM_MODULE nv-platform-win)
elseif(UNIX AND NOT APPLE)
  set(NV_PLATFORM_MODULE nv-platform-linux)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(NV_IS_MOBILE TRUE)
else()
  set(NV_IS_MOBILE FALSE)
endif()

# Tests that need the full stack link against nativeview (defined after platform)
# We use a deferred approach: tests are added after the platform module is built.
# NV_FULL_STACK_TESTS collects test definitions; they are registered at the end.
set(NV_FULL_STACK_TESTS "" CACHE INTERNAL "Tests needing full platform link")

add_subdirectory(modules/nv-core)
add_subdirectory(modules/nv-http)
add_subdirectory(modules/nv-ipc)
add_subdirectory(modules/nv-ops)
add_subdirectory(modules/nv-runtime)
# When building nativeView standalone, set WebView2 fetch root (elkotobi sets it when embedding)
if(NV_WINDOWS_PE_TOOLCHAIN AND NOT DEFINED WEBVIEW2_FETCH_ROOT)
  set(WEBVIEW2_FETCH_ROOT "${CMAKE_BINARY_DIR}" CACHE PATH "Root dir for WebView2 SDK fetch")
endif()
add_subdirectory(modules/${NV_PLATFORM_MODULE})

# =============================================================================
# Convenience alias: "nativeview" target = platform module + full stack
# =============================================================================

add_library(nativeview INTERFACE)
# MinGW GNU ld resolves static .a left-to-right once. CMake still orders
# nv-platform-* before nv-runtime on the link line (dependency graph), so
# nv_window_platform_* stay undefined. Group the in-tree archives so ld
# rescans until satisfied. Keep WebView2 + Win32 libs on nv-platform-*
# (pulled next to that target inside the group; DLL import lib is fine here).
# Same issue on Linux with GNU ld.
if((WIN32 OR (UNIX AND NOT APPLE)) AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(nativeview INTERFACE
    "-Wl,--start-group"
    nv-runtime
    ${NV_PLATFORM_MODULE}
    nv-ops
    nv-ipc
    nv-core
    "-Wl,--end-group"
  )
else()
  target_link_libraries(nativeview INTERFACE ${NV_PLATFORM_MODULE})
endif()

# =============================================================================
# Shared library: nativeview.dll / libnativeview.so / libnativeview.dylib
#
# Build with -DNV_BUILD_SHARED=ON.  Compiles with NV_SHARED_BUILD so NV_API
# exports symbols.  Pascal/C consumers linking the DLL should define NV_SHARED
# when compiling code that includes nativeview headers (imports on Windows).
# Static linking continues to use the `nativeview` INTERFACE target (unchanged).
# =============================================================================

if(NV_BUILD_SHARED)
  add_library(nativeview_shared SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/nv_link_shim.c
  )
  target_compile_definitions(nativeview_shared PRIVATE NV_SHARED_BUILD)
  target_include_directories(nativeview_shared
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )
  set_target_properties(nativeview_shared PROPERTIES
    OUTPUT_NAME nativeview
  )

  if(APPLE)
    foreach(_nv_slib nv-core nv-ipc nv-ops nv-runtime ${NV_PLATFORM_MODULE})
      target_link_options(nativeview_shared PRIVATE
        "LINKER:-force_load,$<TARGET_FILE:${_nv_slib}>"
      )
    endforeach()
    target_link_libraries(nativeview_shared PRIVATE ${NV_PLATFORM_MODULE})
  elseif(MSVC)
    foreach(_nv_slib nv-core nv-ipc nv-ops nv-runtime ${NV_PLATFORM_MODULE})
      target_link_libraries(nativeview_shared PRIVATE
        "/WHOLEARCHIVE:$<TARGET_FILE:${_nv_slib}>"
      )
    endforeach()
    if(NV_WINDOWS_PE_TOOLCHAIN)
      target_link_libraries(nativeview_shared PRIVATE
        ${WEBVIEW2_LIBRARY}
        user32
        ole32
        shell32
        uuid
        runtimeobject
        propsys
        shlwapi
        gdiplus
      )
    endif()
  else()
    # MinGW, Linux (GCC/Clang with GNU or LLD)
    target_link_libraries(nativeview_shared PRIVATE
      "-Wl,--whole-archive"
      nv-runtime
      ${NV_PLATFORM_MODULE}
      nv-ops
      nv-ipc
      nv-core
      "-Wl,--no-whole-archive"
    )
    target_link_libraries(nativeview_shared PRIVATE ${NV_PLATFORM_MODULE})
  endif()
endif()

option(NV_BUILD_JAVA_JNI "Build desktop Java JNI library (nativeview_jni; requires NV_BUILD_SHARED)" OFF)
if(NV_BUILD_JAVA_JNI)
  if(NOT NV_BUILD_SHARED)
    message(FATAL_ERROR "NV_BUILD_JAVA_JNI requires NV_BUILD_SHARED=ON")
  endif()
  if(NV_IS_MOBILE)
    message(FATAL_ERROR "NV_BUILD_JAVA_JNI is only for desktop host builds")
  endif()
  add_subdirectory(bindings/java)
endif()

# =============================================================================
# Planned UI modules — added optionally so the build does not break when
# sources are not yet present. Order matters: nv-ui before nv-designer,
# both before nv-sdk.
# =============================================================================

if(NV_BUILD_UI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/CMakeLists.txt")
  add_subdirectory(modules/nv-ui)
endif()
if(NV_BUILD_UI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-designer/CMakeLists.txt")
  add_subdirectory(modules/nv-designer)
endif()
if(NV_BUILD_UI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-sdk/CMakeLists.txt")
  add_subdirectory(modules/nv-sdk)
endif()

# =============================================================================
# Tests
#
# nv-core tests: self-contained, registered by modules/nv-core/CMakeLists.txt
# nv-ipc/nv-ops/nv-runtime tests: need full platform stack, registered here.
# Set NV_BUILD_TESTS=OFF when embedding (e.g. from elkotobi) to avoid building tests.
# =============================================================================

if(NV_IS_MOBILE)
  set(_nv_build_tests_default OFF)
else()
  set(_nv_build_tests_default ON)
endif()
option(NV_BUILD_TESTS "Build nativeView unit tests" ${_nv_build_tests_default})

if(NV_BUILD_TESTS AND NOT NV_IS_MOBILE)
enable_testing()

set(NV_FULL_STACK_TESTS
  test_ipc
  test_window
  test_window_manager
  test_integration
  test_main_thread
  test_platform_api_dispatch
  test_op_app
  test_op_app_menu
  test_op_clipboard
  test_op_dialog
  test_op_fs
  test_op_fs_watch
  test_op_fs_watch_integration
  test_op_ipc_bus
  test_op_notification
  test_op_screen
  test_op_shell
  test_op_tray
  test_op_window
  test_op_window_menu
  test_op_hotkey
  test_op_windows
)

# Map each test to its module source directory
set(test_ipc_DIR              modules/nv-ipc)
set(test_window_DIR           modules/nv-runtime)
set(test_window_manager_DIR   modules/nv-runtime)
set(test_integration_DIR      modules/nv-runtime)
set(test_main_thread_DIR      modules/nv-runtime)
set(test_platform_api_dispatch_DIR modules/nv-runtime)
set(test_op_app_DIR           modules/nv-ops)
set(test_op_app_menu_DIR      modules/nv-ops)
set(test_op_clipboard_DIR     modules/nv-ops)
set(test_op_dialog_DIR        modules/nv-ops)
set(test_op_fs_DIR            modules/nv-ops)
set(test_op_fs_watch_DIR            modules/nv-ops)
set(test_op_fs_watch_integration_DIR modules/nv-ops)
set(test_op_ipc_bus_DIR       modules/nv-ops)
set(test_op_notification_DIR  modules/nv-ops)
set(test_op_screen_DIR        modules/nv-ops)
set(test_op_shell_DIR         modules/nv-ops)
set(test_op_tray_DIR          modules/nv-ops)
set(test_op_window_DIR        modules/nv-ops)
set(test_op_window_menu_DIR   modules/nv-ops)
set(test_op_hotkey_DIR        modules/nv-ops)
set(test_op_windows_DIR       modules/nv-ops)

foreach(t ${NV_FULL_STACK_TESTS})
  set(t_dir ${${t}_DIR})
  add_executable(${t} ${t_dir}/tests/${t}.c ${t_dir}/tests/test_helpers.c)
  target_link_libraries(${t} nativeview)
  target_include_directories(${t} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ops/src
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-runtime/src
  )
  target_compile_definitions(${t} PRIVATE NV_TEST)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
endif()

# =============================================================================
# Benchmarks
# =============================================================================

if(NOT NV_IS_MOBILE)
  add_executable(bench_ipc benchmarks/bench_ipc.c)
  target_link_libraries(bench_ipc nativeview)
  target_include_directories(bench_ipc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  add_executable(bench_shm benchmarks/bench_shm.c)
  target_link_libraries(bench_shm nv-ipc)
  target_include_directories(bench_shm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  if(APPLE)
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
      set(_nv_ios_sim_arch "arm64")
    else()
      set(_nv_ios_sim_arch "x86_64")
    endif()

    set(_nv_ios_smoke_dir "${CMAKE_CURRENT_SOURCE_DIR}/examples/ios_runner/_xcode")
    add_custom_target(ios_smoke_build
      COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR} -B ${_nv_ios_smoke_dir}
        -G Xcode
        -DCMAKE_SYSTEM_NAME=iOS
        -DCMAKE_OSX_SYSROOT=iphonesimulator
        -DCMAKE_OSX_ARCHITECTURES=${_nv_ios_sim_arch}
        -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
        -DNV_BUILD_TESTS=OFF
        -DNV_BUILD_UI=OFF
        -DNV_BUILD_SHARED=OFF
      COMMAND ${CMAKE_COMMAND} --build ${_nv_ios_smoke_dir} --target ios_runner
      COMMAND ${CMAKE_COMMAND} --build ${_nv_ios_smoke_dir} --target ios_runner_tests
      COMMAND ${CMAKE_COMMAND} --build ${_nv_ios_smoke_dir} --target ios_runner_scheme_fix
    )
  else()
    add_custom_target(ios_smoke_build
      COMMAND ${CMAKE_COMMAND} -E echo "ios_smoke_build is only supported on Apple hosts"
      COMMAND ${CMAKE_COMMAND} -E false
    )
  endif()

  set(_nv_android_ndk "$ENV{ANDROID_NDK}")
  if(NOT _nv_android_ndk)
    set(_nv_android_ndk "$ENV{ANDROID_NDK_HOME}")
  endif()
  if(_nv_android_ndk)
    set(_nv_android_toolchain "${_nv_android_ndk}/build/cmake/android.toolchain.cmake")
    set(_nv_android_smoke_dir "${CMAKE_CURRENT_SOURCE_DIR}/examples/android_runner/_cmake_smoke")
    add_custom_target(android_smoke_build
      COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR} -B ${_nv_android_smoke_dir}
        -DCMAKE_TOOLCHAIN_FILE=${_nv_android_toolchain}
        -DANDROID_ABI=arm64-v8a
        -DANDROID_PLATFORM=android-24
        -DNV_BUILD_TESTS=OFF
        -DNV_BUILD_UI=OFF
        -DNV_BUILD_SHARED=OFF
      COMMAND ${CMAKE_COMMAND} --build ${_nv_android_smoke_dir} --target nv-platform-android
    )
  else()
    add_custom_target(android_smoke_build
      COMMAND ${CMAKE_COMMAND} -E echo "ANDROID_NDK or ANDROID_NDK_HOME is not set"
      COMMAND ${CMAKE_COMMAND} -E false
    )
  endif()
endif()

# =============================================================================
# Examples
# =============================================================================

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(_nv_ios_runner_plist "${CMAKE_CURRENT_BINARY_DIR}/ios_runner_Info.plist")
  file(WRITE "${_nv_ios_runner_plist}" [=[
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleExecutable</key>
  <string>$(EXECUTABLE_NAME)</string>
  <key>CFBundleIdentifier</key>
  <string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleName</key>
  <string>$(PRODUCT_NAME)</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>1.0</string>
  <key>CFBundleVersion</key>
  <string>1</string>
  <key>LSRequiresIPhoneOS</key>
  <true/>
  <key>UIApplicationSceneManifest</key>
  <dict>
    <key>UIApplicationSupportsMultipleScenes</key>
    <false/>
    <key>UISceneConfigurations</key>
    <dict>
      <key>UIWindowSceneSessionRoleApplication</key>
      <array>
        <dict>
          <key>UISceneConfigurationName</key>
          <string>Default Configuration</string>
          <key>UISceneDelegateClassName</key>
          <string>NVIOSSceneDelegate</string>
        </dict>
      </array>
    </dict>
  </dict>
</dict>
</plist>
]=])

  set(_nv_ios_runner_tests_plist "${CMAKE_CURRENT_BINARY_DIR}/ios_runner_tests_Info.plist")
  file(WRITE "${_nv_ios_runner_tests_plist}" [=[
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleExecutable</key>
  <string>$(EXECUTABLE_NAME)</string>
  <key>CFBundleIdentifier</key>
  <string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleName</key>
  <string>$(PRODUCT_NAME)</string>
  <key>CFBundlePackageType</key>
  <string>BNDL</string>
  <key>CFBundleShortVersionString</key>
  <string>1.0</string>
  <key>CFBundleVersion</key>
  <string>1</string>
</dict>
</plist>
]=])

  add_executable(ios_runner MACOSX_BUNDLE examples/ios_runner/main.m)
  target_link_libraries(ios_runner PRIVATE nativeview)
  target_include_directories(ios_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  set_target_properties(ios_runner PROPERTIES
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "14.0"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.nativeview.iosRunner"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    MACOSX_BUNDLE_INFO_PLIST "${_nv_ios_runner_plist}"
  )

  add_library(ios_runner_tests MODULE examples/ios_runner/ios_smoke_tests.m)
  target_link_libraries(ios_runner_tests PRIVATE
    nativeview
    "-framework XCTest"
    "-framework Foundation"
    "-framework UIKit"
  )
  target_include_directories(ios_runner_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  set_target_properties(ios_runner_tests PROPERTIES
    BUNDLE TRUE
    XCTEST TRUE
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "14.0"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.nativeview.iosRunnerTests"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
    XCODE_ATTRIBUTE_PRODUCT_TYPE "com.apple.product-type.bundle.unit-test"
    XCODE_ATTRIBUTE_WRAPPER_EXTENSION "xctest"
    XCODE_ATTRIBUTE_SKIP_INSTALL "YES"
    XCODE_ATTRIBUTE_TEST_HOST "$(BUILT_PRODUCTS_DIR)/ios_runner.app/ios_runner"
    XCODE_ATTRIBUTE_BUNDLE_LOADER "$(BUILT_PRODUCTS_DIR)/ios_runner.app/ios_runner"
    MACOSX_BUNDLE_INFO_PLIST "${_nv_ios_runner_tests_plist}"
  )

  set_property(TARGET ios_runner PROPERTY XCODE_GENERATE_SCHEME YES)
  set_property(TARGET ios_runner_tests PROPERTY XCODE_GENERATE_SCHEME YES)

  set(_nv_xcodeproj_path "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.xcodeproj")
  file(RELATIVE_PATH _nv_xcodeproj_rel "${CMAKE_SOURCE_DIR}" "${_nv_xcodeproj_path}")

  set(_nv_ios_scheme_fix_script "${CMAKE_CURRENT_BINARY_DIR}/nv_fix_ios_runner_scheme.cmake")
  file(WRITE "${_nv_ios_scheme_fix_script}" [=[
if(NOT DEFINED NV_PBXPROJ OR NOT DEFINED NV_SCHEME OR NOT DEFINED NV_CONTAINER)
  message(FATAL_ERROR "nv_fix_ios_runner_scheme: missing NV_PBXPROJ/NV_SCHEME/NV_CONTAINER")
endif()

file(STRINGS "${NV_PBXPROJ}" _pbxproj_lines)

set(_id_runner "")
set(_id_tests "")
set(_in_native_target FALSE)
foreach(_line IN LISTS _pbxproj_lines)
  if(_line MATCHES "^/\\* Begin PBXNativeTarget section \\*/")
    set(_in_native_target TRUE)
  elseif(_line MATCHES "^/\\* End PBXNativeTarget section \\*/")
    set(_in_native_target FALSE)
  endif()

  if(_in_native_target)
    if(_id_runner STREQUAL "" AND _line MATCHES "/\\* ios_runner \\*/ = \\{")
      string(STRIP "${_line}" _line_s)
      string(REGEX MATCH "^[0-9A-Z]+" _id_runner "${_line_s}")
    endif()
    if(_id_tests STREQUAL "" AND _line MATCHES "/\\* ios_runner_tests \\*/ = \\{")
      string(STRIP "${_line}" _line_s)
      string(REGEX MATCH "^[0-9A-Z]+" _id_tests "${_line_s}")
    endif()
  endif()
endforeach()

if(_id_runner STREQUAL "" OR _id_tests STREQUAL "")
  message(FATAL_ERROR "nv_fix_ios_runner_scheme: failed to locate blueprint IDs (ios_runner='${_id_runner}', ios_runner_tests='${_id_tests}')")
endif()

get_filename_component(_scheme_dir "${NV_SCHEME}" DIRECTORY)
file(MAKE_DIRECTORY "${_scheme_dir}")

file(WRITE "${NV_SCHEME}" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
file(APPEND "${NV_SCHEME}" "<Scheme\n")
file(APPEND "${NV_SCHEME}" "   LastUpgradeVersion=\"2650\"\n")
file(APPEND "${NV_SCHEME}" "   version=\"1.3\"\n")
file(APPEND "${NV_SCHEME}" "   >\n")
file(APPEND "${NV_SCHEME}" "   <BuildAction\n")
file(APPEND "${NV_SCHEME}" "      parallelizeBuildables=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "      buildImplicitDependencies=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "      >\n")
file(APPEND "${NV_SCHEME}" "      <BuildActionEntries>\n")
file(APPEND "${NV_SCHEME}" "         <BuildActionEntry\n")
file(APPEND "${NV_SCHEME}" "            buildForTesting=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "            buildForRunning=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "            buildForProfiling=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "            buildForArchiving=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "            buildForAnalyzing=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "            >\n")
file(APPEND "${NV_SCHEME}" "            <BuildableReference\n")
file(APPEND "${NV_SCHEME}" "               BuildableIdentifier=\"primary\"\n")
file(APPEND "${NV_SCHEME}" "               BlueprintIdentifier=\"${_id_runner}\"\n")
file(APPEND "${NV_SCHEME}" "               BuildableName=\"ios_runner\"\n")
file(APPEND "${NV_SCHEME}" "               BlueprintName=\"ios_runner\"\n")
file(APPEND "${NV_SCHEME}" "               ReferencedContainer=\"container:${NV_CONTAINER}\"/>\n")
file(APPEND "${NV_SCHEME}" "         </BuildActionEntry>\n")
file(APPEND "${NV_SCHEME}" "      </BuildActionEntries>\n")
file(APPEND "${NV_SCHEME}" "   </BuildAction>\n")
file(APPEND "${NV_SCHEME}" "   <TestAction\n")
file(APPEND "${NV_SCHEME}" "      buildConfiguration=\"Debug\"\n")
file(APPEND "${NV_SCHEME}" "      selectedDebuggerIdentifier=\"Xcode.DebuggerFoundation.Debugger.LLDB\"\n")
file(APPEND "${NV_SCHEME}" "      selectedLauncherIdentifier=\"Xcode.DebuggerFoundation.Launcher.LLDB\"\n")
file(APPEND "${NV_SCHEME}" "      shouldUseLaunchSchemeArgsEnv=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "      >\n")
file(APPEND "${NV_SCHEME}" "      <Testables>\n")
file(APPEND "${NV_SCHEME}" "         <TestableReference skipped=\"NO\">\n")
file(APPEND "${NV_SCHEME}" "            <BuildableReference\n")
file(APPEND "${NV_SCHEME}" "               BuildableIdentifier=\"primary\"\n")
file(APPEND "${NV_SCHEME}" "               BlueprintIdentifier=\"${_id_tests}\"\n")
file(APPEND "${NV_SCHEME}" "               BuildableName=\"ios_runner_tests.xctest/ios_runner_tests\"\n")
file(APPEND "${NV_SCHEME}" "               BlueprintName=\"ios_runner_tests\"\n")
file(APPEND "${NV_SCHEME}" "               ReferencedContainer=\"container:${NV_CONTAINER}\"/>\n")
file(APPEND "${NV_SCHEME}" "         </TestableReference>\n")
file(APPEND "${NV_SCHEME}" "      </Testables>\n")
file(APPEND "${NV_SCHEME}" "      <MacroExpansion>\n")
file(APPEND "${NV_SCHEME}" "         <BuildableReference\n")
file(APPEND "${NV_SCHEME}" "            BuildableIdentifier=\"primary\"\n")
file(APPEND "${NV_SCHEME}" "            BlueprintIdentifier=\"${_id_runner}\"\n")
file(APPEND "${NV_SCHEME}" "            BuildableName=\"ios_runner\"\n")
file(APPEND "${NV_SCHEME}" "            BlueprintName=\"ios_runner\"\n")
file(APPEND "${NV_SCHEME}" "            ReferencedContainer=\"container:${NV_CONTAINER}\"/>\n")
file(APPEND "${NV_SCHEME}" "      </MacroExpansion>\n")
file(APPEND "${NV_SCHEME}" "      <AdditionalOptions/>\n")
file(APPEND "${NV_SCHEME}" "   </TestAction>\n")
file(APPEND "${NV_SCHEME}" "   <LaunchAction\n")
file(APPEND "${NV_SCHEME}" "      buildConfiguration=\"Debug\"\n")
file(APPEND "${NV_SCHEME}" "      selectedDebuggerIdentifier=\"Xcode.DebuggerFoundation.Debugger.LLDB\"\n")
file(APPEND "${NV_SCHEME}" "      selectedLauncherIdentifier=\"Xcode.DebuggerFoundation.Launcher.LLDB\"\n")
file(APPEND "${NV_SCHEME}" "      launchStyle=\"0\"\n")
file(APPEND "${NV_SCHEME}" "      useCustomWorkingDirectory=\"NO\"\n")
file(APPEND "${NV_SCHEME}" "      ignoresPersistentStateOnLaunch=\"NO\"\n")
file(APPEND "${NV_SCHEME}" "      debugDocumentVersioning=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "      debugServiceExtension=\"internal\"\n")
file(APPEND "${NV_SCHEME}" "      allowLocationSimulation=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "      >\n")
file(APPEND "${NV_SCHEME}" "      <BuildableProductRunnable\n")
file(APPEND "${NV_SCHEME}" "         runnableDebuggingMode=\"0\"\n")
file(APPEND "${NV_SCHEME}" "         >\n")
file(APPEND "${NV_SCHEME}" "         <BuildableReference\n")
file(APPEND "${NV_SCHEME}" "            BuildableIdentifier=\"primary\"\n")
file(APPEND "${NV_SCHEME}" "            BlueprintIdentifier=\"${_id_runner}\"\n")
file(APPEND "${NV_SCHEME}" "            BuildableName=\"ios_runner\"\n")
file(APPEND "${NV_SCHEME}" "            BlueprintName=\"ios_runner\"\n")
file(APPEND "${NV_SCHEME}" "            ReferencedContainer=\"container:${NV_CONTAINER}\"/>\n")
file(APPEND "${NV_SCHEME}" "      </BuildableProductRunnable>\n")
file(APPEND "${NV_SCHEME}" "      <AdditionalOptions/>\n")
file(APPEND "${NV_SCHEME}" "   </LaunchAction>\n")
file(APPEND "${NV_SCHEME}" "   <ProfileAction\n")
file(APPEND "${NV_SCHEME}" "      buildConfiguration=\"Release\"\n")
file(APPEND "${NV_SCHEME}" "      shouldUseLaunchSchemeArgsEnv=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "      savedToolIdentifier=\"\"\n")
file(APPEND "${NV_SCHEME}" "      useCustomWorkingDirectory=\"NO\"\n")
file(APPEND "${NV_SCHEME}" "      debugDocumentVersioning=\"YES\"\n")
file(APPEND "${NV_SCHEME}" "      >\n")
file(APPEND "${NV_SCHEME}" "      <BuildableProductRunnable\n")
file(APPEND "${NV_SCHEME}" "         runnableDebuggingMode=\"0\"\n")
file(APPEND "${NV_SCHEME}" "         >\n")
file(APPEND "${NV_SCHEME}" "         <BuildableReference\n")
file(APPEND "${NV_SCHEME}" "            BuildableIdentifier=\"primary\"\n")
file(APPEND "${NV_SCHEME}" "            BlueprintIdentifier=\"${_id_runner}\"\n")
file(APPEND "${NV_SCHEME}" "            BuildableName=\"ios_runner\"\n")
file(APPEND "${NV_SCHEME}" "            BlueprintName=\"ios_runner\"\n")
file(APPEND "${NV_SCHEME}" "            ReferencedContainer=\"container:${NV_CONTAINER}\"/>\n")
file(APPEND "${NV_SCHEME}" "      </BuildableProductRunnable>\n")
file(APPEND "${NV_SCHEME}" "   </ProfileAction>\n")
file(APPEND "${NV_SCHEME}" "   <AnalyzeAction\n")
file(APPEND "${NV_SCHEME}" "      buildConfiguration=\"Debug\"/>\n")
file(APPEND "${NV_SCHEME}" "   <ArchiveAction\n")
file(APPEND "${NV_SCHEME}" "      buildConfiguration=\"Release\"\n")
file(APPEND "${NV_SCHEME}" "      revealArchiveInOrganizer=\"YES\"/>\n")
file(APPEND "${NV_SCHEME}" "</Scheme>\n")
]=])

  add_custom_target(ios_runner_scheme_fix
    COMMAND ${CMAKE_COMMAND}
      -DNV_PBXPROJ="${_nv_xcodeproj_path}/project.pbxproj"
      -DNV_SCHEME="${_nv_xcodeproj_path}/xcshareddata/xcschemes/ios_runner.xcscheme"
      -DNV_CONTAINER="${_nv_xcodeproj_rel}"
      -P "${_nv_ios_scheme_fix_script}"
    DEPENDS ios_runner ios_runner_tests
  )
elseif(NOT NV_IS_MOBILE)
  add_executable(hello examples/hello/main.c)
  target_link_libraries(hello nativeview)
  target_include_directories(hello PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
endif()

# counter_ui — nv-ui component model end-to-end example
if(NOT NV_IS_MOBILE AND NV_BUILD_UI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/CMakeLists.txt")
  add_executable(counter_ui examples/counter_ui/main.c)
  target_link_libraries(counter_ui PRIVATE nativeview nv-ui)
  target_include_directories(counter_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/include
  )
  # vue_slot — NV_COMP_VUE slot with createApp per slot
  add_executable(vue_slot examples/vue_slot/main.c)
  target_link_libraries(vue_slot PRIVATE nativeview nv-ui)
  target_include_directories(vue_slot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/include
  )
  # todo_app — SQLite + dynamic list (CMake in examples/todo_app/c_todo; shared Vue UI in examples/todo_app/ui)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/examples/todo_app/c_todo")
  # settings_form — all widget types (label, input, checkbox, select, slider, textarea)
  add_executable(settings_form examples/settings_form/main.c)
  target_link_libraries(settings_form PRIVATE nativeview nv-ui)
  target_include_directories(settings_form PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/include
  )
  # live_data — slider + live display (Phase 4 use case; shm path when platform supports)
  add_executable(live_data examples/live_data/main.c)
  target_link_libraries(live_data PRIVATE nativeview nv-ui)
  target_include_directories(live_data PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/nv-ui/include
  )
endif()

# Demo
if(NOT NV_IS_MOBILE)
  add_subdirectory(demo)
endif()

# =============================================================================
# pkg-config
# =============================================================================

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/nativeview.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/nativeview.pc
  @ONLY
)

# =============================================================================
# Amalgamation Target
# =============================================================================

if(NV_IS_MOBILE)
  set(AMALG_PLATFORM "")
elseif(APPLE)
  set(AMALG_PLATFORM mac)
elseif(WIN32)
  set(AMALG_PLATFORM win)
else()
  set(AMALG_PLATFORM linux)
endif()

if(NOT NV_IS_MOBILE)
  add_custom_target(amalgamate
    COMMAND ${CMAKE_COMMAND} -E echo "Running amalgamate script for ${AMALG_PLATFORM}..."
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/amalgamate.py --platform ${AMALG_PLATFORM} --output dist
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating single-file amalgamation"
  )
endif()

# =============================================================================
# Installation
# =============================================================================

install(
  TARGETS nativeview
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)

if(NV_BUILD_SHARED)
  install(
    TARGETS nativeview_shared
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
  )
endif()

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION include
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/nativeview.pc
  DESTINATION lib/pkgconfig
)
