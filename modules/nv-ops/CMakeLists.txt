# =============================================================================
# nv-ops — Native operation handlers
#
# Provides: file system, dialog, clipboard, shell, screen, tray,
#           notification, window ops, IPC bus, and op registry.
#
# Dependencies: nv-ipc → nv-core
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-ops C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(NV_ROOT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set(NV_ROOT_SRC     "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

if(NOT TARGET nv-ipc)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nv-ipc
                   ${CMAKE_CURRENT_BINARY_DIR}/nv-ipc)
endif()

add_library(nv-ops STATIC
  src/nv_op_app.c
  src/nv_op_app_menu.c
  src/nv_op_clipboard.c
  src/nv_op_dialog.c
  src/nv_op_fs.c
  src/nv_op_fs_watch.c
  src/nv_op_ipc_bus.c
  src/nv_op_notification.c
  src/nv_op_registry.c
  src/nv_op_screen.c
  src/nv_op_shell.c
  src/nv_op_tray.c
  src/nv_op_window.c
  src/nv_op_window_hotkey.c
  src/nv_op_windows.c
)

target_include_directories(nv-ops
  PUBLIC
    ${NV_ROOT_INCLUDE}
  PRIVATE
    ${NV_ROOT_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nv-ops PUBLIC nv-ipc)

# =============================================================================
# Tests
# =============================================================================

enable_testing()

set(NV_OPS_TESTS
  test_op_app
  test_op_app_menu
  test_op_clipboard
  test_op_dialog
  test_op_fs
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

# Op tests are registered by the root CMakeLists after the platform target is defined,
# because they need the full platform stack (nv_mac_*/nv_win_* symbols).
# When building nv-ops standalone (no root), fall back to nv-ops only.
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # Built as part of the monorepo — tests registered by root CMakeLists.txt
else()
  # Standalone build: link against nv-ops only (platform symbols will be missing)
  foreach(t ${NV_OPS_TESTS})
    add_executable(${t} tests/${t}.c tests/test_helpers.c)
    target_link_libraries(${t} nv-ops)
    target_include_directories(${t} PRIVATE ${NV_ROOT_INCLUDE} ${NV_ROOT_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(${t} PRIVATE NV_TEST)
    add_test(NAME ${t} COMMAND ${t})
  endforeach()
endif()
