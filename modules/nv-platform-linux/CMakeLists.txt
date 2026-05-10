# =============================================================================
# nv-platform-linux — Linux WebKitGTK backend
#
# Provides: WebKitGTK integration, GTK window management, GObject memory model.
#
# Dependencies: nv-runtime → nv-ops → nv-ipc → nv-core
# Requires: Linux, webkit2gtk-4.1
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-platform-linux C)

if(NOT (UNIX AND NOT APPLE))
  message(FATAL_ERROR "nv-platform-linux is Linux only")
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

find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)

pkg_check_modules(NV_AYATANA_APPINDICATOR ayatana-appindicator-0.1)
if(NOT NV_AYATANA_APPINDICATOR_FOUND)
  pkg_check_modules(NV_AYATANA_APPINDICATOR appindicator3-0.1)
endif()

pkg_check_modules(NV_LIBNOTIFY libnotify)
pkg_check_modules(NV_X11 x11)

add_library(nv-platform-linux STATIC
  src/nv_linux.c
  src/nv_linux_fs_watch.c
  src/nv_linux_hotkey.c
  src/nv_linux_tray.c
)

target_include_directories(nv-platform-linux
  PUBLIC
    ${NV_ROOT_INCLUDE}
  PRIVATE
    ${NV_ROOT_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/../nv-ops/src
    ${WEBKIT_INCLUDE_DIRS}
)

# JSC (JavaScriptCore) headers live under the same webkitgtk prefix
# but are not exposed by pkg-config; derive from WEBKIT_INCLUDE_DIRS.
foreach(_wi_dir ${WEBKIT_INCLUDE_DIRS})
  if(_wi_dir MATCHES "webkitgtk")
    get_filename_component(_wk_prefix "${_wi_dir}" DIRECTORY)
    list(APPEND _jsc_inc "${_wk_prefix}/webkitgtk-4.1")
    break()
  endif()
endforeach()
if(_jsc_inc)
  target_include_directories(nv-platform-linux PRIVATE ${_jsc_inc})
endif()

target_link_libraries(nv-platform-linux
  PUBLIC
    nv-runtime
    ${WEBKIT_LIBRARIES}
)

if(NV_AYATANA_APPINDICATOR_FOUND)
  target_compile_definitions(nv-platform-linux PRIVATE NV_HAS_APPINDICATOR)
  target_include_directories(nv-platform-linux PRIVATE ${NV_AYATANA_APPINDICATOR_INCLUDE_DIRS})
  target_link_libraries(nv-platform-linux PUBLIC ${NV_AYATANA_APPINDICATOR_LIBRARIES})
endif()

if(NV_LIBNOTIFY_FOUND)
  target_compile_definitions(nv-platform-linux PRIVATE NV_HAS_LIBNOTIFY)
  target_include_directories(nv-platform-linux PRIVATE ${NV_LIBNOTIFY_INCLUDE_DIRS})
  target_link_libraries(nv-platform-linux PUBLIC ${NV_LIBNOTIFY_LIBRARIES})
endif()

if(NV_X11_FOUND)
  target_include_directories(nv-platform-linux PRIVATE ${NV_X11_INCLUDE_DIRS})
  target_link_libraries(nv-platform-linux PUBLIC ${NV_X11_LIBRARIES})
endif()
