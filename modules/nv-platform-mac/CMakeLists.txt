# =============================================================================
# nv-platform-mac — macOS WebView backend
#
# Provides: WKWebView integration, NSWindow management, ARC memory model.
#
# Dependencies: nv-runtime → nv-ops → nv-ipc → nv-core
# Requires: macOS, Cocoa.framework, WebKit.framework
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-platform-mac C)

if(NOT APPLE)
  message(FATAL_ERROR "nv-platform-mac is macOS only")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(NV_ROOT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set(NV_ROOT_SRC     "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

if(NOT TARGET nv-runtime)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nv-runtime
                   ${CMAKE_CURRENT_BINARY_DIR}/nv-runtime)
endif()

add_library(nv-platform-mac STATIC
  src/nv_mac.m
  src/nv_mac_menu.m
  src/nv_mac_fs_watch.m
  src/nv_mac_hotkey.m
)

set_source_files_properties(src/nv_mac.m PROPERTIES
  COMPILE_FLAGS "-x objective-c"
)
set_source_files_properties(src/nv_mac_menu.m PROPERTIES
  COMPILE_FLAGS "-x objective-c"
)
set_source_files_properties(src/nv_mac_fs_watch.m PROPERTIES
  COMPILE_FLAGS "-x objective-c"
)
set_source_files_properties(src/nv_mac_hotkey.m PROPERTIES
  COMPILE_FLAGS "-x objective-c"
)

target_include_directories(nv-platform-mac
  PUBLIC
    ${NV_ROOT_INCLUDE}
  PRIVATE
    ${NV_ROOT_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/../nv-ops/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../nv-runtime/src
)

target_link_libraries(nv-platform-mac
  PUBLIC
    nv-runtime
    "-framework Carbon"
    "-framework Cocoa"
    "-framework CoreServices"
    "-framework WebKit"
    "-framework UserNotifications"
)
