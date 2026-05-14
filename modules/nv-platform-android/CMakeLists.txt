# =============================================================================
# nv-platform-android — Android backend (NDK stub)
#
# Provides: Android platform hooks required by nv-runtime.
#
# NOTE: This is a minimal native stub to allow Android cross builds to link.
# WebView hosting + JNI bridge is implemented in a separate phase.
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-platform-android C)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
  message(FATAL_ERROR "nv-platform-android is Android only (CMAKE_SYSTEM_NAME must be Android)")
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

add_library(nv-platform-android SHARED
  src/nv_android.c
)

find_library(NV_ANDROID_LOG_LIB log)

target_include_directories(nv-platform-android
  PUBLIC
    ${NV_ROOT_INCLUDE}
  PRIVATE
    ${NV_ROOT_SRC}
)

target_link_libraries(nv-platform-android
  PUBLIC
    nv-runtime
    ${NV_ANDROID_LOG_LIB}
)
