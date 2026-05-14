# =============================================================================
# nv-ipc — IPC message routing layer
#
# Provides: JSON-RPC message dispatch, JS script injection helpers.
#
# Dependencies: nv-core
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-ipc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CheckSymbolExists)

set(NV_ROOT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set(NV_ROOT_SRC     "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

# When built standalone, find nv-core via add_subdirectory
if(NOT TARGET nv-core)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nv-core
                   ${CMAKE_CURRENT_BINARY_DIR}/nv-core)
endif()

add_library(nv-ipc STATIC
  src/nv_ipc.c
  src/nv_ipc_script.c
  src/nv_shm.c
)

target_include_directories(nv-ipc
  PUBLIC
    ${NV_ROOT_INCLUDE}
  PRIVATE
    ${NV_ROOT_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nv-ipc PUBLIC nv-core)

check_symbol_exists(shm_open "sys/mman.h;fcntl.h" NV_HAVE_SHM_OPEN)
if(NV_HAVE_SHM_OPEN)
  target_compile_definitions(nv-ipc PRIVATE NV_HAVE_SHM_OPEN=1)
else()
  target_compile_definitions(nv-ipc PRIVATE NV_HAVE_SHM_OPEN=0)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
  target_link_libraries(nv-ipc PUBLIC rt)
endif()

# =============================================================================
# Tests
# =============================================================================

enable_testing()

# test_ipc needs nv-runtime (nv_send, nv_window_get_arena live there)
# test_ipc_script only needs nv-ipc itself
add_executable(test_ipc_script tests/test_ipc_script.c tests/test_helpers.c)
target_link_libraries(test_ipc_script nv-ipc)
add_executable(test_shm tests/test_shm.c)
target_link_libraries(test_shm nv-ipc)
target_include_directories(test_ipc_script PRIVATE ${NV_ROOT_INCLUDE} ${NV_ROOT_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(test_ipc_script PRIVATE NV_TEST)
add_test(NAME test_ipc_script COMMAND test_ipc_script)
add_test(NAME test_shm COMMAND test_shm)

# test_ipc needs full stack — registered by root CMakeLists when built as monorepo
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # Standalone: skip test_ipc (missing runtime symbols)
  message(STATUS "nv-ipc standalone: test_ipc skipped (needs nv-runtime)")
endif()
