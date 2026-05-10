# =============================================================================
# nv-core — Zero-dependency utility layer
#
# Provides: arena allocator, string helpers, JSON parser/composer,
#           general utilities, and structured logging.
#
# Dependencies: none (stdlib only)
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(nv-core C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Root include dir (shared across all modules)
set(NV_ROOT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set(NV_ROOT_SRC     "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

# Generate version header. When nv-core is built standalone (no parent CMakeLists),
# the parent's include(GenerateVersion.cmake) won't have run, so we run it here.
# When built from the parent, the include was already run; running again is harmless
# (it overwrites the same file with the same content).
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/GenerateVersion.cmake")

add_library(nv-core STATIC
  src/nv_arena.c
  src/nv_str.c
  src/nv_json.c
  src/nv_base64.c
  src/nv_util.c
  src/nv_log.c
)

target_include_directories(nv-core
  PUBLIC
    ${NV_ROOT_INCLUDE}
    ${CMAKE_BINARY_DIR}/include
  PRIVATE
    ${NV_ROOT_SRC}
)

# =============================================================================
# Tests
# =============================================================================

enable_testing()

set(NV_CORE_TESTS
  test_arena
  test_str
  test_base64
  test_json
  test_json_parse
  test_json_compose
  test_util
)
# test_log uses unistd.h, pipe, dup2 (POSIX); skip on Windows
if(NOT WIN32)
  list(APPEND NV_CORE_TESTS test_log)
endif()

foreach(t ${NV_CORE_TESTS})
  add_executable(${t} tests/${t}.c tests/test_helpers.c)
  target_link_libraries(${t} nv-core)
  target_include_directories(${t} PRIVATE ${NV_ROOT_INCLUDE} ${NV_ROOT_SRC})
  target_compile_definitions(${t} PRIVATE NV_TEST)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
