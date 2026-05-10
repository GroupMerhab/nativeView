# =============================================================================
# nv-platform-win — Windows WebView2 backend
#
# Provides: WebView2 COM integration, Win32 window management.
#
# Dependencies: nv-runtime → nv-ops → nv-ipc → nv-core
# Requires: Windows, WebView2 SDK (auto-downloaded if missing), user32, ole32
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-platform-win C CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/NvDetectWindowsPeToolchain.cmake)
if(NOT NV_WINDOWS_PE_TOOLCHAIN)
  message(FATAL_ERROR "nv-platform-win is Windows only")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Find or download WebView2 SDK (fetches from NuGet if not installed)
set(WEBVIEW2_FIND_QUIETLY OFF)
set(WEBVIEW2_FIND_REQUIRED ON)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FetchWebView2.cmake)

set(NV_ROOT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set(NV_ROOT_SRC     "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

if(NOT TARGET nv-runtime)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nv-runtime
                   ${CMAKE_CURRENT_BINARY_DIR}/nv-runtime)
endif()

# nv_win_env_options.cpp uses MSVC WRL headers (<wrl/implements.h>); MinGW's WRL
# is incomplete. The helper is unused (app passes NULL env options); ship Win32
# build without it until a WRL-free implementation exists.
add_library(nv-platform-win STATIC
  src/nv_win.c
  src/nv_win_clipboard_image.cpp
  src/nv_win_fs_watch.c
  src/nv_win_hotkey.c
  src/nv_win_notification.c
  src/nv_win_tray.c
  src/nv_win_dialog_file.c
  src/nv_win_dialog_message.c
)

target_include_directories(nv-platform-win
  PUBLIC
    ${NV_ROOT_INCLUDE}
  PRIVATE
    ${NV_ROOT_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/../nv-ops/src
    ${WEBVIEW2_INCLUDE_DIR}
)

# MinGW GNU ld resolves static archives left-to-right. If nv-runtime is PUBLIC,
# CMake may list nv-platform-win before nv-runtime on the final link line, leaving
# nv_window_platform_* undefined. MSVC is unaffected; keep PUBLIC there.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(nv-platform-win
    PRIVATE
      nv-runtime
    PUBLIC
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
else()
  target_link_libraries(nv-platform-win
    PUBLIC
      nv-runtime
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
