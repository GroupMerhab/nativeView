# =============================================================================
# nv-platform-ios — iOS WebView backend
#
# Provides: WKWebView integration, UIWindow management, Objective-C MRC model.
#
# Dependencies: nv-runtime → nv-ops → nv-ipc → nv-core
# Requires: iOS, UIKit.framework, WebKit.framework
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-platform-ios C OBJC)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  message(FATAL_ERROR "nv-platform-ios is iOS only (CMAKE_SYSTEM_NAME must be iOS)")
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

add_library(nv-platform-ios STATIC
  src/nv_ios.m
)

target_include_directories(nv-platform-ios
  PUBLIC
    ${NV_ROOT_INCLUDE}
  PRIVATE
    ${NV_ROOT_SRC}
)

target_link_libraries(nv-platform-ios
  PUBLIC
    nv-runtime
    "-framework Foundation"
    "-framework UIKit"
    "-framework WebKit"
)

set_target_properties(nv-platform-ios PROPERTIES
  XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "14.0"
  XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "NO"
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
  target_compile_options(nv-platform-ios PRIVATE -fno-objc-arc)
endif()
