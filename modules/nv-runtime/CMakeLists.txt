# =============================================================================
# nv-runtime — App and window lifecycle management
#
# Provides: nv_app_t lifecycle, nv_window_t management, window manager.
#           Platform backends link against this module.
#
# Dependencies: nv-ops → nv-ipc → nv-core
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-runtime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(NV_ROOT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set(NV_ROOT_SRC     "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

if(NOT TARGET nv-ops)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nv-ops
                   ${CMAKE_CURRENT_BINARY_DIR}/nv-ops)
endif()

add_library(nv-runtime STATIC
  src/nv_core.c
  src/nv_menu.c
  src/nv_window.c
  src/nv_window_context_menu.c
  src/nv_hotkey_combo.c
  src/nv_hotkey.c
  src/nv_hotkey_stubs.c
  src/nv_window_manager.c
)

target_include_directories(nv-runtime
  PUBLIC
    ${NV_ROOT_INCLUDE}
  PRIVATE
    ${NV_ROOT_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../nv-ops/src
)

target_link_libraries(nv-runtime PUBLIC nv-ops)

# =============================================================================
# Tests
# =============================================================================

enable_testing()

set(NV_RUNTIME_TESTS
  test_window
  test_window_manager
  test_integration
)

# Runtime tests need the full platform stack — registered by root CMakeLists
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # Standalone: link against nv-runtime only
  foreach(t ${NV_RUNTIME_TESTS})
    add_executable(${t} tests/${t}.c tests/test_helpers.c)
    target_link_libraries(${t} nv-runtime)
    target_include_directories(${t} PRIVATE ${NV_ROOT_INCLUDE} ${NV_ROOT_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/../nv-ops/src)
    target_compile_definitions(${t} PRIVATE NV_TEST)
    add_test(NAME ${t} COMMAND ${t})
  endforeach()
endif()
